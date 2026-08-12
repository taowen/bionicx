#include <asm/ptrace.h>
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/*
 * BionicX ABI handoff.
 *
 * Android may report a synthetic SIGSEGV when a Bionic process execs an
 * AArch64 glibc loader from app-private storage.  A short-lived Bionic parent
 * traces only the exec boundary, probes one loader instruction, suppresses
 * that one synthetic stop when present, and detaches.  The application then
 * runs untraced.  This is not syscall translation, chroot, or PRoot.
 */

enum launch_mode { MODE_DIRECT, MODE_LOADER };

struct assignment {
    const char *value;
    int unset;
};

struct options {
    enum launch_mode mode;
    const char *loader;
    const char *library_path;
    const char *cwd;
    const char *argv0;
    struct assignment *assignments;
    size_t assignment_count;
    int diagnose;
    int debug_stop;
    int command_index;
};

static volatile sig_atomic_t shutdown_signal;

static void request_shutdown(int signal_number) {
    shutdown_signal = signal_number;
}

static int install_shutdown_handlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_shutdown;
    sigemptyset(&action.sa_mask);
    return sigaction(SIGTERM, &action, NULL) == 0 &&
                   sigaction(SIGINT, &action, NULL) == 0 &&
                   sigaction(SIGHUP, &action, NULL) == 0
            ? 0 : -1;
}

static int reset_child_signals(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    const int signals[] = {SIGTERM, SIGINT, SIGHUP, SIGPIPE};
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        if (sigaction(signals[i], &action, NULL) != 0) return -1;
    }
    sigset_t mask;
    sigemptyset(&mask);
    return sigprocmask(SIG_SETMASK, &mask, NULL);
}

static void usage(FILE *stream, const char *program) {
    fprintf(stream,
            "Usage: %s [options] -- PROGRAM [ARG...]\n"
            "\n"
            "Modes:\n"
            "  --direct                 exec PROGRAM and use its PT_INTERP (default)\n"
            "  --loader PATH            exec glibc ld.so explicitly\n"
            "  --library-path PATH      pass --library-path to ld.so\n"
            "\n"
            "Process setup:\n"
            "  --cwd PATH               working directory\n"
            "  --env NAME=VALUE         set an environment variable (repeatable)\n"
            "  --unset NAME             remove an environment variable (repeatable)\n"
            "  --argv0 VALUE            override target argv[0]\n"
            "  --diagnose-signals       report fatal registers and ELF mappings\n"
            "  --debug-stop             stop after bootstrap for an external debugger\n",
            program);
}

static int append_assignment(struct options *options, const char *value,
                             int unset) {
    size_t count = options->assignment_count + 1;
    struct assignment *items = realloc(options->assignments,
                                        count * sizeof(*items));
    if (items == NULL) return -1;
    options->assignments = items;
    items[count - 1].value = value;
    items[count - 1].unset = unset;
    options->assignment_count = count;
    return 0;
}

static const char *take_value(int argc, char **argv, int *index,
                              const char *option) {
    if (*index + 1 >= argc) {
        fprintf(stderr, "bionicx-exec: %s requires a value\n", option);
        return NULL;
    }
    return argv[++*index];
}

static int parse_options(int argc, char **argv, struct options *options) {
    memset(options, 0, sizeof(*options));
    options->mode = MODE_DIRECT;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            options->command_index = i + 1;
            break;
        }
        if (strcmp(arg, "--direct") == 0) options->mode = MODE_DIRECT;
        else if (strcmp(arg, "--loader") == 0) {
            options->loader = take_value(argc, argv, &i, arg);
            if (options->loader == NULL) return -1;
            options->mode = MODE_LOADER;
        }
        else if (strcmp(arg, "--library-path") == 0) {
            options->library_path = take_value(argc, argv, &i, arg);
            if (options->library_path == NULL) return -1;
        }
        else if (strcmp(arg, "--cwd") == 0) {
            options->cwd = take_value(argc, argv, &i, arg);
            if (options->cwd == NULL) return -1;
        }
        else if (strcmp(arg, "--argv0") == 0) {
            options->argv0 = take_value(argc, argv, &i, arg);
            if (options->argv0 == NULL) return -1;
        }
        else if (strcmp(arg, "--env") == 0) {
            const char *value = take_value(argc, argv, &i, arg);
            if (value == NULL || strchr(value, '=') == NULL ||
                    append_assignment(options, value, 0) != 0) return -1;
        }
        else if (strcmp(arg, "--unset") == 0) {
            const char *value = take_value(argc, argv, &i, arg);
            if (value == NULL || strchr(value, '=') != NULL ||
                    append_assignment(options, value, 1) != 0) return -1;
        }
        else if (strcmp(arg, "--diagnose-signals") == 0)
            options->diagnose = 1;
        else if (strcmp(arg, "--debug-stop") == 0)
            options->debug_stop = 1;
        else if (strcmp(arg, "--help") == 0) {
            usage(stdout, argv[0]);
            exit(0);
        }
        else {
            fprintf(stderr, "bionicx-exec: unknown option: %s\n", arg);
            return -1;
        }
    }

    if (options->command_index == 0 || options->command_index >= argc) {
        fprintf(stderr, "bionicx-exec: missing PROGRAM after --\n");
        return -1;
    }
    if (options->mode == MODE_LOADER && options->loader == NULL) return -1;
    if (options->mode == MODE_LOADER && options->library_path == NULL) {
        fprintf(stderr, "bionicx-exec: loader mode requires --library-path\n");
        return -1;
    }
    return 0;
}

static int configure_child(const struct options *options) {
    for (size_t i = 0; i < options->assignment_count; ++i) {
        const char *value = options->assignments[i].value;
        if (options->assignments[i].unset) {
            if (unsetenv(value) != 0) return -1;
            continue;
        }
        const char *equals = strchr(value, '=');
        size_t name_length = (size_t)(equals - value);
        char *name = strndup(value, name_length);
        if (name == NULL) return -1;
        int result = setenv(name, equals + 1, 1);
        free(name);
        if (result != 0) return -1;
    }
    return options->cwd == NULL || chdir(options->cwd) == 0 ? 0 : -1;
}

static char **build_child_argv(int argc, char **argv,
                               const struct options *options,
                               const char **exec_path) {
    int target_count = argc - options->command_index;
    size_t prefix = options->mode == MODE_LOADER
            ? (options->argv0 != NULL ? 5 : 3) : 0;
    char **child = calloc(prefix + (size_t)target_count + 1, sizeof(*child));
    if (child == NULL) return NULL;

    size_t out = 0;
    if (options->mode == MODE_LOADER) {
        *exec_path = options->loader;
        child[out++] = (char *)options->loader;
        child[out++] = (char *)"--library-path";
        child[out++] = (char *)options->library_path;
        if (options->argv0 != NULL) {
            child[out++] = (char *)"--argv0";
            child[out++] = (char *)options->argv0;
        }
    }
    else *exec_path = argv[options->command_index];

    for (int i = 0; i < target_count; ++i)
        child[out++] = argv[options->command_index + i];
    if (options->mode == MODE_DIRECT && options->argv0 != NULL)
        child[0] = (char *)options->argv0;
    child[out] = NULL;
    return child;
}

static int wait_for_stop(pid_t child, int expected, const char *stage) {
    int status = 0;
    do {
        if (waitpid(child, &status, 0) == child) break;
    } while (errno == EINTR);
    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "bionicx-exec: %s did not stop (status=0x%x)\n",
                stage, status);
        return -1;
    }
    if (expected != 0 && WSTOPSIG(status) != expected) {
        fprintf(stderr, "bionicx-exec: %s signal=%d expected=%d\n",
                stage, WSTOPSIG(status), expected);
        return -1;
    }
    return status;
}

static void report_address_mapping(pid_t child, const char *name,
                                   uint64_t address) {
    if (address == 0) return;

    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%ld/maps", (long)child);
    FILE *maps = fopen(maps_path, "re");
    if (maps == NULL) {
        fprintf(stderr, "bionicx-exec: %s maps unavailable: %s\n",
                name, strerror(errno));
        return;
    }

    char *line = NULL;
    size_t capacity = 0;
    while (getline(&line, &capacity, maps) >= 0) {
        unsigned long long start = 0;
        unsigned long long end = 0;
        unsigned long long offset = 0;
        char permissions[5] = {0};
        if (sscanf(line, "%llx-%llx %4s %llx", &start, &end,
                   permissions, &offset) != 4 || address < start ||
                address >= end) {
            continue;
        }

        size_t length = strlen(line);
        if (length != 0 && line[length - 1] == '\n') line[length - 1] = '\0';
        fprintf(stderr,
                "bionicx-exec: %s mapping=%s file-offset=0x%llx\n",
                name, line, offset + (unsigned long long)address - start);
        break;
    }
    free(line);
    fclose(maps);
}

static void report_fatal_signal(pid_t child, int signal_number) {
    struct user_pt_regs regs;
    struct iovec io = {.iov_base = &regs, .iov_len = sizeof(regs)};
    siginfo_t info;
    memset(&regs, 0, sizeof(regs));
    memset(&info, 0, sizeof(info));
    ptrace(PTRACE_GETREGSET, child, (void *)(long)NT_PRSTATUS, &io);
    ptrace(PTRACE_GETSIGINFO, child, NULL, &info);
    fprintf(stderr,
            "bionicx-exec: signal=%d syscall=%" PRIu64
            " pc=0x%" PRIx64 " lr=0x%" PRIx64
            " code=%d address=%p\n",
            signal_number, (uint64_t)regs.regs[8], (uint64_t)regs.pc,
            (uint64_t)regs.regs[30], info.si_code, info.si_addr);
    fprintf(stderr,
            "bionicx-exec: registers x0=0x%" PRIx64 " x1=0x%" PRIx64
            " x2=0x%" PRIx64 " x3=0x%" PRIx64 " x4=0x%" PRIx64
            " x5=0x%" PRIx64 " x6=0x%" PRIx64 " x7=0x%" PRIx64
            " sp=0x%" PRIx64 "\n",
            (uint64_t)regs.regs[0], (uint64_t)regs.regs[1],
            (uint64_t)regs.regs[2], (uint64_t)regs.regs[3],
            (uint64_t)regs.regs[4], (uint64_t)regs.regs[5],
            (uint64_t)regs.regs[6], (uint64_t)regs.regs[7],
            (uint64_t)regs.sp);
    report_address_mapping(child, "pc", (uint64_t)regs.pc);
    report_address_mapping(child, "lr", (uint64_t)regs.regs[30]);
    report_address_mapping(child, "fault", (uint64_t)(uintptr_t)info.si_addr);
    fflush(stderr);
}

static int is_handled_android_seccomp_probe(pid_t child) {
    struct user_pt_regs regs;
    struct iovec io = {.iov_base = &regs, .iov_len = sizeof(regs)};
    memset(&regs, 0, sizeof(regs));
    if (ptrace(PTRACE_GETREGSET, child, (void *)(long)NT_PRSTATUS, &io) != 0)
        return 0;
#ifdef SYS_landlock_create_ruleset
    if (regs.regs[8] == SYS_landlock_create_ruleset) return 1;
#endif
#ifdef SYS_name_to_handle_at
    if (regs.regs[8] == SYS_name_to_handle_at) return 1;
#endif
    return 0;
}

static int diagnose_signals(pid_t child) {
    int status;
    if (ptrace(PTRACE_SETOPTIONS, child, NULL,
               (void *)(long)(PTRACE_O_TRACEEXEC | PTRACE_O_TRACEFORK |
                              PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE)) != 0)
        return 19;
    if (ptrace(PTRACE_CONT, child, NULL, NULL) != 0) return 20;
    for (;;) {
        pid_t stopped = waitpid(-1, &status, __WALL);
        if (stopped < 0) {
            if (errno == EINTR) continue;
            return 21;
        }
        if (WIFEXITED(status)) {
            if (stopped == child) return WEXITSTATUS(status);
            continue;
        }
        if (WIFSIGNALED(status)) {
            if (stopped == child) return 128 + WTERMSIG(status);
            continue;
        }
        if (!WIFSTOPPED(status)) continue;
        int signal_number = WSTOPSIG(status);
        unsigned int ptrace_event = (unsigned int)status >> 16;
        if (signal_number == SIGTRAP && ptrace_event != 0) {
            if (ptrace(PTRACE_CONT, stopped, NULL, NULL) != 0) return 22;
            continue;
        }
        /* The Android compatibility preload intentionally converts these
         * optional, seccomp-trapped probes to ENOSYS. Let its SIGSYS handler
         * run so signal diagnosis can reach a later, genuine application
         * failure. Unknown SIGSYS remains fatal and fully reported. */
        if (signal_number == SIGSYS &&
                is_handled_android_seccomp_probe(stopped)) {
            if (ptrace(PTRACE_CONT, stopped, NULL,
                       (void *)(long)SIGSYS) != 0) return 22;
            continue;
        }
        /* Bootstrap single-step traps have already been consumed before this
         * loop. A later SIGTRAP is an application crash/breakpoint (Chromium
         * uses a deliberate trap for CHECK failures); suppressing it repeats
         * the same instruction forever and hides the decisive PC. */
        if (signal_number == SIGTRAP || signal_number == SIGSYS ||
                signal_number == SIGSEGV ||
                signal_number == SIGBUS || signal_number == SIGILL ||
                signal_number == SIGABRT) {
            fprintf(stderr, "bionicx-exec: fatal pid=%ld primary=%ld\n",
                    (long)stopped, (long)child);
            report_fatal_signal(stopped, signal_number);
            kill(-child, SIGKILL);
            ptrace(PTRACE_KILL, stopped, NULL, NULL);
            return 128 + signal_number;
        }
        int delivered = signal_number == SIGSTOP ? 0 : signal_number;
        if (ptrace(PTRACE_CONT, stopped, NULL,
                   (void *)(long)delivered) != 0) return 22;
    }
}

static void signal_session(pid_t primary, int signal_number) {
    if (primary > 0) kill(-primary, signal_number);
    /* One Android application UID owns one BionicX display session.  Linux's
     * pid=-1 form reaches detached descendants even after setsid(), while
     * excluding this supervisor itself. */
    kill(-1, signal_number);
}

static int reap_and_check_empty(void) {
    for (;;) {
        int status;
        pid_t result = waitpid(-1, &status, WNOHANG);
        if (result > 0) continue;
        if (result == 0) return 0;
        if (errno == EINTR) continue;
        return errno == ECHILD;
    }
}

static void terminate_session(pid_t primary, int signal_number) {
    fprintf(stderr, "bionicx-exec: stopping session signal=%d\n",
            signal_number);
    signal_session(primary, SIGTERM);
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (reap_and_check_empty()) return;
        signal_session(primary, SIGTERM);
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000};
        nanosleep(&delay, NULL);
    }
    signal_session(primary, SIGKILL);
    for (;;) {
        int status;
        pid_t result = waitpid(-1, &status, 0);
        if (result > 0) continue;
        if (result < 0 && errno == EINTR) continue;
        break;
    }
}

static int supervise_session(pid_t primary) {
    int primary_status = 0;
    int primary_exited = 0;
    unsigned int session_children = 0;

    for (;;) {
        int status = 0;
        pid_t exited = waitpid(-1, &status, 0);
        if (exited < 0) {
            if (errno == EINTR) {
                if (shutdown_signal != 0) {
                    int signal_number = shutdown_signal;
                    terminate_session(primary, signal_number);
                    return 128 + signal_number;
                }
                continue;
            }
            if (errno == ECHILD && primary_exited) break;
            fprintf(stderr, "bionicx-exec: session wait failed: %s\n",
                    strerror(errno));
            return 1;
        }

        if (exited == primary) {
            primary_status = status;
            primary_exited = 1;
            fprintf(stderr,
                    "bionicx-exec: primary pid=%d exited status=0x%x; "
                    "draining session\n",
                    primary, status);
        }
        else {
            ++session_children;
            fprintf(stderr,
                    "bionicx-exec: session child pid=%d exited status=0x%x\n",
                    exited, status);
        }
    }

    fprintf(stderr, "bionicx-exec: session drained children=%u\n",
            session_children);
    if (WIFEXITED(primary_status)) return WEXITSTATUS(primary_status);
    if (WIFSIGNALED(primary_status)) return 128 + WTERMSIG(primary_status);
    return 1;
}

int main(int argc, char **argv) {
    struct options options;
    if (parse_options(argc, argv, &options) != 0) {
        usage(stderr, argv[0]);
        return 2;
    }

    const char *exec_path = NULL;
    char **child_argv = build_child_argv(argc, argv, &options, &exec_path);
    if (child_argv == NULL) return 111;

    if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0) {
        fprintf(stderr, "bionicx-exec: cannot become child subreaper: %s\n",
                strerror(errno));
        return 111;
    }

    pid_t child = fork();
    if (child < 0) return 111;
    if (child == 0) {
        if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0) != 0 ||
                getppid() == 1) _exit(119);
        if (reset_child_signals() != 0) _exit(118);
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) != 0) _exit(120);
        raise(SIGSTOP);
        if (configure_child(&options) != 0) {
            fprintf(stderr, "bionicx-exec: process setup failed: %s\n",
                    strerror(errno));
            _exit(121);
        }
        execv(exec_path, child_argv);
        fprintf(stderr, "bionicx-exec: execv(%s): %s\n",
                exec_path, strerror(errno));
        _exit(errno == EACCES ? 126 : 127);
    }

    free(child_argv);
    if (setpgid(child, child) != 0 && errno != EACCES) {
        fprintf(stderr, "bionicx-exec: setpgid(%d): %s\n", child,
                strerror(errno));
        kill(child, SIGKILL);
        return 111;
    }
    if (wait_for_stop(child, SIGSTOP, "initial stop") < 0) return 3;
    if (ptrace(PTRACE_SETOPTIONS, child, NULL,
               (void *)(long)PTRACE_O_TRACEEXEC) != 0) return 4;
    if (ptrace(PTRACE_CONT, child, NULL, NULL) != 0) return 5;

    int status = wait_for_stop(child, SIGTRAP, "exec event");
    if (status < 0 || ((unsigned)status >> 16) != PTRACE_EVENT_EXEC) return 6;
    if (ptrace(PTRACE_SINGLESTEP, child, NULL, NULL) != 0) return 7;
    status = wait_for_stop(child, 0, "loader probe");
    if (status < 0) return 8;
    if (WSTOPSIG(status) == SIGSEGV) {
        siginfo_t info;
        memset(&info, 0, sizeof(info));
        if (ptrace(PTRACE_GETSIGINFO, child, NULL, &info) != 0) return 9;
        fprintf(stderr,
                "bionicx-exec: suppressed loader SIGSEGV code=%d address=%p\n",
                info.si_code, info.si_addr);
        if (ptrace(PTRACE_SINGLESTEP, child, NULL, NULL) != 0) return 10;
        if (wait_for_stop(child, SIGTRAP, "loader retry") < 0) return 11;
    }
    else if (WSTOPSIG(status) != SIGTRAP) return 12;

    if (options.diagnose) return diagnose_signals(child);
    if (ptrace(PTRACE_DETACH, child, NULL, NULL) != 0) return 13;

    if (install_shutdown_handlers() != 0) {
        fprintf(stderr, "bionicx-exec: cannot install signal handlers: %s\n",
                strerror(errno));
        kill(-child, SIGKILL);
        return 111;
    }

    if (options.debug_stop) {
        kill(child, SIGSTOP);
        do {
            status = 0;
        } while (waitpid(child, &status, WUNTRACED) < 0 && errno == EINTR);
        if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) return 14;
        printf("bionicx-exec: debug-stop pid=%d; send SIGCONT to resume\n", child);
        fflush(stdout);
    }

    printf("bionicx-exec: running untraced pid=%d\n", child);
    fflush(stdout);
    free(options.assignments);
    return supervise_session(child);
}
