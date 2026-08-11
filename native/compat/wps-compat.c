#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
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

static void remember_stream_failure(const char *operation, const char *path) {
    snprintf(last_failed_stream, sizeof(last_failed_stream), "%s:%s",
             operation, path ? path : "<null>");
    fprintf(stderr, "wps-sysvipc-compat: %s failed errno=%d path=%s\n",
            operation, errno, path ? path : "<null>");
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
    if (result == NULL) remember_stream_failure("fopen", path);
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
    if (result == NULL) remember_stream_failure("fopen64", path);
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
            fprintf(stderr, "wps-sysvipc-compat: popen via Android shell: %s\n",
                    command);
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
