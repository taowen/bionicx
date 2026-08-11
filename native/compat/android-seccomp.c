#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

static int is_optional_probe(long number) {
#ifdef SYS_landlock_create_ruleset
    if (number == SYS_landlock_create_ruleset) return 1;
#endif
#ifdef SYS_name_to_handle_at
    if (number == SYS_name_to_handle_at) return 1;
#endif
    return 0;
}

static void handle_seccomp_trap(int signal_number, siginfo_t *info,
                                void *raw_context) {
    (void)signal_number;
    if (is_optional_probe(info->si_syscall)) {
#if defined(__aarch64__)
        ucontext_t *context = raw_context;
        context->uc_mcontext.regs[0] = (uint64_t)-ENOSYS;
        return;
#endif
    }
    static const char message[] = "BIONICX unknown Android seccomp trap\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(128 + SIGSYS);
}

__attribute__((constructor)) static void install_seccomp_compatibility(void) {
    struct sigaction action = {0};
    action.sa_sigaction = handle_seccomp_trap;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    (void)sigaction(SIGSYS, &action, NULL);
}
