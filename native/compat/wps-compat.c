#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/*
 * Small userspace replacement for the one-element System V semaphores used
 * by Qt/WPS.  Android app seccomp traps the real arm64 semget/semop/semctl
 * syscalls.  WPS uses them for process coordination, not kernel IPC with an
 * unrelated program, so keeping the state inside the WPS process is enough.
 */

#define SLOT_COUNT 64
#define SEM_ID_BASE 0x575000

struct semaphore_slot {
    int used;
    key_t key;
    int value;
    unsigned short wait_negative;
    unsigned short wait_zero;
    pthread_cond_t changed;
};

static pthread_mutex_t slots_lock = PTHREAD_MUTEX_INITIALIZER;
static struct semaphore_slot slots[SLOT_COUNT];
static __thread char last_failed_stream[512];

struct popen_slot {
    FILE *stream;
    pid_t child;
};
static pthread_mutex_t popen_lock = PTHREAD_MUTEX_INITIALIZER;
static struct popen_slot popen_slots[SLOT_COUNT];

__attribute__((constructor)) static void debugger_rendezvous(void) {
    if (getenv("WPS_DEBUG_STOP") != NULL) {
        static const char message[] =
                "wps-sysvipc-compat: stopping after glibc initialization\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        raise(SIGSTOP);
    }
}

static int trace_files_enabled(void);
static int trace_commands_enabled(void);

static void remember_stream_failure(const char *operation, const char *path,
                                    void *caller) {
    snprintf(last_failed_stream, sizeof(last_failed_stream), "%s:%s",
             operation, path ? path : "<null>");
    Dl_info module;
    memset(&module, 0, sizeof(module));
    if (dladdr(caller, &module) != 0 && module.dli_fbase != NULL) {
        fprintf(stderr,
                "wps-sysvipc-compat: %s failed errno=%d path=%s "
                "caller=%s+0x%lx\n",
                operation, errno, path ? path : "<null>",
                module.dli_fname ? module.dli_fname : "<anonymous>",
                (unsigned long)((uintptr_t)caller -
                                (uintptr_t)module.dli_fbase));
    } else {
        fprintf(stderr,
                "wps-sysvipc-compat: %s failed errno=%d path=%s caller=%p\n",
                operation, errno, path ? path : "<null>", caller);
    }
    if (trace_files_enabled() && path != NULL && path[0] == '\0') {
        void *frames[16];
        int count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
        for (int i = 1; i < count; ++i) {
            Dl_info frame;
            memset(&frame, 0, sizeof(frame));
            if (dladdr(frames[i], &frame) != 0 && frame.dli_fbase != NULL) {
                fprintf(stderr, "wps-file-trace: frame=%d module=%s+0x%lx\n",
                        i, frame.dli_fname ? frame.dli_fname : "<anonymous>",
                        (unsigned long)((uintptr_t)frames[i] -
                                        (uintptr_t)frame.dli_fbase));
            } else {
                fprintf(stderr, "wps-file-trace: frame=%d address=%p\n", i,
                        frames[i]);
            }
        }
    }
}

static int trace_files_enabled(void) {
    static int initialized;
    static int enabled;
    if (!initialized) {
        enabled = getenv("BIONICX_TRACE_FILES") != NULL;
        initialized = 1;
    }
    return enabled;
}

static int trace_commands_enabled(void) {
    static int initialized;
    static int enabled;
    if (!initialized) {
        enabled = getenv("BIONICX_TRACE_COMMANDS") != NULL;
        initialized = 1;
    }
    return enabled;
}

static void trace_command_caller(const char *command, const char *mode,
                                 void *caller) {
    Dl_info module;
    memset(&module, 0, sizeof(module));
    if (dladdr(caller, &module) != 0 && module.dli_fbase != NULL) {
        fprintf(stderr,
                "wps-sysvipc-compat: popen via Android shell: %s mode=%s "
                "caller=%s+0x%lx\n",
                command, mode,
                module.dli_fname ? module.dli_fname : "<anonymous>",
                (unsigned long)((uintptr_t)caller -
                                (uintptr_t)module.dli_fbase));
    } else {
        fprintf(stderr,
                "wps-sysvipc-compat: popen via Android shell: %s mode=%s "
                "caller=%p\n",
                command, mode, caller);
    }
    if (!trace_commands_enabled()) return;
    void *frames[16];
    int count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    for (int i = 1; i < count; ++i) {
        Dl_info frame;
        memset(&frame, 0, sizeof(frame));
        if (dladdr(frames[i], &frame) != 0 && frame.dli_fbase != NULL) {
            fprintf(stderr, "wps-command-trace: frame=%d module=%s+0x%lx\n",
                    i, frame.dli_fname ? frame.dli_fname : "<anonymous>",
                    (unsigned long)((uintptr_t)frames[i] -
                                    (uintptr_t)frame.dli_fbase));
        } else {
            fprintf(stderr, "wps-command-trace: frame=%d address=%p\n", i,
                    frames[i]);
        }
    }
}

static void trace_command_arguments(const char *operation, const char *path,
                                    char *const arguments[]) {
    if (!trace_commands_enabled()) return;
    fprintf(stderr, "wps-command-trace: %s path=%s", operation,
            path ? path : "<null>");
    if (arguments != NULL) {
        for (int i = 0; arguments[i] != NULL && i < 8; ++i)
            fprintf(stderr, " argv[%d]=%s", i, arguments[i]);
    }
    fprintf(stderr, "\n");
}

int execve(const char *path, char *const arguments[],
           char *const environment[]) {
    static int (*next_execve)(const char *, char *const[], char *const[]);
    if (next_execve == NULL)
        next_execve = (int (*)(const char *, char *const[], char *const[]))
                dlsym(RTLD_NEXT, "execve");
    trace_command_arguments("execve", path, arguments);
    if (next_execve == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return next_execve(path, arguments, environment);
}

int execv(const char *path, char *const arguments[]) {
    static int (*next_execv)(const char *, char *const[]);
    if (next_execv == NULL)
        next_execv = (int (*)(const char *, char *const[]))dlsym(RTLD_NEXT,
                                                                  "execv");
    trace_command_arguments("execv", path, arguments);
    if (next_execv == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return next_execv(path, arguments);
}

int execvp(const char *file, char *const arguments[]) {
    static int (*next_execvp)(const char *, char *const[]);
    if (next_execvp == NULL)
        next_execvp = (int (*)(const char *, char *const[]))dlsym(RTLD_NEXT,
                                                                   "execvp");
    trace_command_arguments("execvp", file, arguments);
    if (next_execvp == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return next_execvp(file, arguments);
}

int posix_spawn(pid_t *pid, const char *path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attributes, char *const arguments[],
                char *const environment[]) {
    static int (*next_posix_spawn)(pid_t *, const char *,
                                   const posix_spawn_file_actions_t *,
                                   const posix_spawnattr_t *, char *const[],
                                   char *const[]);
    if (next_posix_spawn == NULL)
        next_posix_spawn = (int (*)(pid_t *, const char *,
                                    const posix_spawn_file_actions_t *,
                                    const posix_spawnattr_t *, char *const[],
                                    char *const[]))dlsym(RTLD_NEXT,
                                                        "posix_spawn");
    trace_command_arguments("posix_spawn", path, arguments);
    if (next_posix_spawn == NULL) return ENOSYS;
    return next_posix_spawn(pid, path, file_actions, attributes, arguments,
                            environment);
}

int posix_spawnp(pid_t *pid, const char *file,
                 const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attributes, char *const arguments[],
                 char *const environment[]) {
    static int (*next_posix_spawnp)(pid_t *, const char *,
                                    const posix_spawn_file_actions_t *,
                                    const posix_spawnattr_t *, char *const[],
                                    char *const[]);
    if (next_posix_spawnp == NULL)
        next_posix_spawnp = (int (*)(pid_t *, const char *,
                                     const posix_spawn_file_actions_t *,
                                     const posix_spawnattr_t *, char *const[],
                                     char *const[]))dlsym(RTLD_NEXT,
                                                         "posix_spawnp");
    trace_command_arguments("posix_spawnp", file, arguments);
    if (next_posix_spawnp == NULL) return ENOSYS;
    return next_posix_spawnp(pid, file, file_actions, attributes, arguments,
                             environment);
}

int system(const char *command) {
    static int (*next_system)(const char *);
    if (next_system == NULL)
        next_system = (int (*)(const char *))dlsym(RTLD_NEXT, "system");
    if (trace_commands_enabled())
        fprintf(stderr, "wps-command-trace: system command=%s\n",
                command ? command : "<null>");
    if (next_system == NULL) {
        errno = ENOSYS;
        return -1;
    }
    return next_system(command);
}

static void trace_file_pair(const char *operation, const char *old_path,
                            const char *new_path, int result,
                            int saved_errno) {
    if (!trace_files_enabled()) return;
    int displayed_errno = result == 0 ? 0 : saved_errno;
    fprintf(stderr,
            "wps-file-trace: %s old=%s new=%s result=%d errno=%d (%s)\n",
            operation, old_path ? old_path : "<null>",
            new_path ? new_path : "<null>", result, displayed_errno,
            result == 0 ? "success" : strerror(displayed_errno));
}

static void trace_temporary_file(const char *operation, const char *path,
                                 int result, int saved_errno) {
    if (!trace_files_enabled()) return;
    int displayed_errno = result >= 0 ? 0 : saved_errno;
    fprintf(stderr,
            "wps-file-trace: %s template=%s result=%d errno=%d (%s)\n",
            operation, path ? path : "<null>", result, displayed_errno,
            result >= 0 ? "success" : strerror(displayed_errno));
}

int mkstemp(char *path) {
    static int (*next_mkstemp)(char *);
    if (next_mkstemp == NULL)
        next_mkstemp = (int (*)(char *))dlsym(RTLD_NEXT, "mkstemp");
    if (next_mkstemp == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_mkstemp(path);
    int saved_errno = errno;
    trace_temporary_file("mkstemp", path, result, saved_errno);
    errno = saved_errno;
    return result;
}

int mkstemps(char *path, int suffix_length) {
    static int (*next_mkstemps)(char *, int);
    if (next_mkstemps == NULL)
        next_mkstemps = (int (*)(char *, int))dlsym(RTLD_NEXT, "mkstemps");
    if (next_mkstemps == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_mkstemps(path, suffix_length);
    int saved_errno = errno;
    trace_temporary_file("mkstemps", path, result, saved_errno);
    errno = saved_errno;
    return result;
}

int mkostemp(char *path, int flags) {
    static int (*next_mkostemp)(char *, int);
    if (next_mkostemp == NULL)
        next_mkostemp = (int (*)(char *, int))dlsym(RTLD_NEXT, "mkostemp");
    if (next_mkostemp == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_mkostemp(path, flags);
    int saved_errno = errno;
    trace_temporary_file("mkostemp", path, result, saved_errno);
    errno = saved_errno;
    return result;
}

int mkostemps(char *path, int suffix_length, int flags) {
    static int (*next_mkostemps)(char *, int, int);
    if (next_mkostemps == NULL)
        next_mkostemps = (int (*)(char *, int, int))dlsym(RTLD_NEXT,
                                                             "mkostemps");
    if (next_mkostemps == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_mkostemps(path, suffix_length, flags);
    int saved_errno = errno;
    trace_temporary_file("mkostemps", path, result, saved_errno);
    errno = saved_errno;
    return result;
}

int link(const char *old_path, const char *new_path) {
    static int (*next_link)(const char *, const char *);
    if (next_link == NULL)
        next_link = (int (*)(const char *, const char *))dlsym(RTLD_NEXT,
                                                                "link");
    if (next_link == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_link(old_path, new_path);
    int saved_errno = errno;
    trace_file_pair("link", old_path, new_path, result, saved_errno);
    errno = saved_errno;
    return result;
}

int linkat(int old_directory, const char *old_path, int new_directory,
           const char *new_path, int flags) {
    static int (*next_linkat)(int, const char *, int, const char *, int);
    if (next_linkat == NULL)
        next_linkat = (int (*)(int, const char *, int, const char *, int))
                dlsym(RTLD_NEXT, "linkat");
    if (next_linkat == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_linkat(old_directory, old_path, new_directory, new_path,
                             flags);
    int saved_errno = errno;
    if (trace_files_enabled()) {
        int displayed_errno = result == 0 ? 0 : saved_errno;
        fprintf(stderr,
                "wps-file-trace: linkat olddir=%d old=%s newdir=%d new=%s "
                "flags=0x%x result=%d errno=%d (%s)\n",
                old_directory, old_path, new_directory, new_path, flags,
                result, displayed_errno,
                result == 0 ? "success" : strerror(displayed_errno));
    }
    errno = saved_errno;
    return result;
}

int rename(const char *old_path, const char *new_path) {
    static int (*next_rename)(const char *, const char *);
    if (next_rename == NULL)
        next_rename = (int (*)(const char *, const char *))dlsym(RTLD_NEXT,
                                                                  "rename");
    if (next_rename == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_rename(old_path, new_path);
    int saved_errno = errno;
    trace_file_pair("rename", old_path, new_path, result, saved_errno);
    errno = saved_errno;
    return result;
}

int renameat(int old_directory, const char *old_path, int new_directory,
             const char *new_path) {
    static int (*next_renameat)(int, const char *, int, const char *);
    if (next_renameat == NULL)
        next_renameat = (int (*)(int, const char *, int, const char *))
                dlsym(RTLD_NEXT, "renameat");
    if (next_renameat == NULL) {
        errno = ENOSYS;
        return -1;
    }
    int result = next_renameat(old_directory, old_path, new_directory,
                               new_path);
    int saved_errno = errno;
    if (trace_files_enabled()) {
        int displayed_errno = result == 0 ? 0 : saved_errno;
        fprintf(stderr,
                "wps-file-trace: renameat olddir=%d old=%s newdir=%d new=%s "
                "result=%d errno=%d (%s)\n",
                old_directory, old_path, new_directory, new_path, result,
                displayed_errno,
                result == 0 ? "success" : strerror(displayed_errno));
    }
    errno = saved_errno;
    return result;
}

FILE *fopen(const char *path, const char *mode) {
    static FILE *(*next_fopen)(const char *, const char *);
    if (next_fopen == NULL)
        next_fopen = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT,
                                                                  "fopen");
    if (next_fopen == NULL) {
        errno = ENOSYS;
        return NULL;
    }
    FILE *result = next_fopen(path, mode);
    if (result == NULL)
        remember_stream_failure("fopen", path, __builtin_return_address(0));
    return result;
}

FILE *fopen64(const char *path, const char *mode) {
    static FILE *(*next_fopen64)(const char *, const char *);
    if (next_fopen64 == NULL)
        next_fopen64 = (FILE *(*)(const char *, const char *))dlsym(
                RTLD_NEXT, "fopen64");
    if (next_fopen64 == NULL) {
        errno = ENOSYS;
        return NULL;
    }
    FILE *result = next_fopen64(path, mode);
    if (result == NULL)
        remember_stream_failure("fopen64", path, __builtin_return_address(0));
    return result;
}

FILE *popen(const char *command, const char *mode) {
    if (command == NULL || mode == NULL ||
            !((mode[0] == 'r' || mode[0] == 'w') &&
              (mode[1] == '\0' || (mode[1] == 'e' && mode[2] == '\0')))) {
        errno = EINVAL;
        return NULL;
    }
    int descriptors[2];
    if (pipe(descriptors) != 0) return NULL;
    pid_t child = fork();
    if (child < 0) {
        int saved_errno = errno;
        close(descriptors[0]);
        close(descriptors[1]);
        errno = saved_errno;
        return NULL;
    }
    if (child == 0) {
        if (mode[0] == 'r') {
            dup2(descriptors[1], STDOUT_FILENO);
        } else {
            dup2(descriptors[0], STDIN_FILENO);
        }
        close(descriptors[0]);
        close(descriptors[1]);
        unsetenv("LD_PRELOAD");
        unsetenv("LD_LIBRARY_PATH");
        execl("/system/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    int stream_fd = mode[0] == 'r' ? descriptors[0] : descriptors[1];
    close(mode[0] == 'r' ? descriptors[1] : descriptors[0]);
    FILE *stream = fdopen(stream_fd, mode[0] == 'r' ? "r" : "w");
    if (stream == NULL) {
        int saved_errno = errno;
        close(stream_fd);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        errno = saved_errno;
        return NULL;
    }

    pthread_mutex_lock(&popen_lock);
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (popen_slots[i].stream == NULL) {
            popen_slots[i].stream = stream;
            popen_slots[i].child = child;
            pthread_mutex_unlock(&popen_lock);
            trace_command_caller(command, mode, __builtin_return_address(0));
            return stream;
        }
    }
    pthread_mutex_unlock(&popen_lock);
    fclose(stream);
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    errno = EMFILE;
    return NULL;
}

int pclose(FILE *stream) {
    pid_t child = -1;
    pthread_mutex_lock(&popen_lock);
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (popen_slots[i].stream == stream) {
            child = popen_slots[i].child;
            popen_slots[i].stream = NULL;
            popen_slots[i].child = 0;
            break;
        }
    }
    pthread_mutex_unlock(&popen_lock);
    if (child < 0) {
        errno = EINVAL;
        return -1;
    }
    fclose(stream);
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return status;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"
int getc(FILE *stream) {
    static int (*next_getc)(FILE *);
    if (stream == NULL) {
        fprintf(stderr,
                "wps-sysvipc-compat: guarded getc(NULL) caller=%p last=%s\n",
                __builtin_return_address(0),
                last_failed_stream[0] ? last_failed_stream : "<none>");
        errno = EBADF;
        return EOF;
    }
    if (next_getc == NULL)
        next_getc = (int (*)(FILE *))dlsym(RTLD_NEXT, "getc");
    if (next_getc == NULL) {
        errno = ENOSYS;
        return EOF;
    }
    return next_getc(stream);
}
#pragma GCC diagnostic pop

static struct semaphore_slot *find_slot(int semid) {
    int index = semid - SEM_ID_BASE;
    if (index < 0 || index >= SLOT_COUNT || !slots[index].used) {
        errno = EINVAL;
        return NULL;
    }
    return &slots[index];
}

int semget(key_t key, int nsems, int semflg) {
    if (nsems != 1) {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&slots_lock);
    if (key != IPC_PRIVATE) {
        for (int i = 0; i < SLOT_COUNT; ++i) {
            if (slots[i].used && slots[i].key == key) {
                if ((semflg & IPC_CREAT) && (semflg & IPC_EXCL)) {
                    pthread_mutex_unlock(&slots_lock);
                    errno = EEXIST;
                    return -1;
                }
                pthread_mutex_unlock(&slots_lock);
                return SEM_ID_BASE + i;
            }
        }
        if (!(semflg & IPC_CREAT)) {
            pthread_mutex_unlock(&slots_lock);
            errno = ENOENT;
            return -1;
        }
    }
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (!slots[i].used) {
            slots[i].used = 1;
            slots[i].key = key;
            slots[i].value = 0;
            slots[i].wait_negative = 0;
            slots[i].wait_zero = 0;
            pthread_cond_init(&slots[i].changed, NULL);
            pthread_mutex_unlock(&slots_lock);
            fprintf(stderr, "wps-sysvipc-compat: emulating sem key=0x%x id=%d\n",
                    (unsigned)key, SEM_ID_BASE + i);
            return SEM_ID_BASE + i;
        }
    }
    pthread_mutex_unlock(&slots_lock);
    errno = ENOSPC;
    return -1;
}

int semctl(int semid, int semnum, int cmd, ...) {
    (void)semnum;
    va_list arguments;
    va_start(arguments, cmd);
    pthread_mutex_lock(&slots_lock);
    struct semaphore_slot *slot = find_slot(semid);
    if (slot == NULL) {
        pthread_mutex_unlock(&slots_lock);
        va_end(arguments);
        return -1;
    }

    int result = 0;
    fprintf(stderr, "wps-sysvipc-compat: semctl id=%d cmd=%d\n", semid, cmd);
    switch (cmd) {
        case SETVAL:
            slot->value = va_arg(arguments, int);
            pthread_cond_broadcast(&slot->changed);
            break;
        case GETVAL:
            result = slot->value;
            break;
        case GETPID:
            result = 0;
            break;
        case GETNCNT:
            result = slot->wait_negative;
            break;
        case GETZCNT:
            result = slot->wait_zero;
            break;
        case IPC_RMID:
            slot->used = 0;
            pthread_cond_broadcast(&slot->changed);
            pthread_cond_destroy(&slot->changed);
            break;
        default:
            result = -1;
            errno = EINVAL;
            break;
    }
    pthread_mutex_unlock(&slots_lock);
    va_end(arguments);
    return result;
}

static int apply_operations(int semid, struct sembuf *operations,
                            size_t count, const struct timespec *timeout) {
    fprintf(stderr, "wps-sysvipc-compat: semop id=%d count=%zu first=%d\n",
            semid, count, count ? operations[0].sem_op : 0);
    pthread_mutex_lock(&slots_lock);
    struct semaphore_slot *slot = find_slot(semid);
    if (slot == NULL) {
        pthread_mutex_unlock(&slots_lock);
        return -1;
    }

    for (;;) {
        int possible = 1;
        int resulting_value = slot->value;
        for (size_t i = 0; i < count; ++i) {
            if (operations[i].sem_num != 0) {
                errno = EFBIG;
                possible = -1;
                break;
            }
            if (operations[i].sem_op < 0 &&
                    resulting_value < -operations[i].sem_op) {
                possible = 0;
                slot->wait_negative++;
                break;
            }
            if (operations[i].sem_op == 0 && resulting_value != 0) {
                possible = 0;
                slot->wait_zero++;
                break;
            }
            resulting_value += operations[i].sem_op;
        }
        if (possible > 0) {
            slot->value = resulting_value;
            pthread_cond_broadcast(&slot->changed);
            pthread_mutex_unlock(&slots_lock);
            return 0;
        }
        if (possible < 0) {
            pthread_mutex_unlock(&slots_lock);
            return -1;
        }
        int nowait = 0;
        for (size_t i = 0; i < count; ++i)
            if (operations[i].sem_flg & IPC_NOWAIT) nowait = 1;
        if (nowait) {
            pthread_mutex_unlock(&slots_lock);
            errno = EAGAIN;
            return -1;
        }
        int wait_result = timeout
                ? pthread_cond_timedwait(&slot->changed, &slots_lock, timeout)
                : pthread_cond_wait(&slot->changed, &slots_lock);
        slot->wait_negative = 0;
        slot->wait_zero = 0;
        if (wait_result == ETIMEDOUT) {
            pthread_mutex_unlock(&slots_lock);
            errno = EAGAIN;
            return -1;
        }
    }
}

int semop(int semid, struct sembuf *operations, size_t count) {
    return apply_operations(semid, operations, count, NULL);
}

int semtimedop(int semid, struct sembuf *operations, size_t count,
               const struct timespec *relative_timeout) {
    struct timespec absolute;
    if (clock_gettime(CLOCK_REALTIME, &absolute) != 0) return -1;
    absolute.tv_sec += relative_timeout->tv_sec;
    absolute.tv_nsec += relative_timeout->tv_nsec;
    if (absolute.tv_nsec >= 1000000000L) {
        absolute.tv_sec++;
        absolute.tv_nsec -= 1000000000L;
    }
    return apply_operations(semid, operations, count, &absolute);
}
