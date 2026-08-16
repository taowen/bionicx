#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

static int is_android_seccomp_probe(long number) {
#ifdef SYS_clone3
    /* glibc pthread_create selects clone when clone3 is unavailable. */
    if (number == SYS_clone3) return 1;
#endif
#ifdef SYS_landlock_create_ruleset
    if (number == SYS_landlock_create_ruleset) return 1;
#endif
#ifdef SYS_name_to_handle_at
    if (number == SYS_name_to_handle_at) return 1;
#endif
#ifdef SYS_set_robust_list
    if (number == SYS_set_robust_list) return 1;
#endif
#ifdef SYS_shmget
    if (number == SYS_shmget) return 1;
#endif
#ifdef SYS_shmctl
    if (number == SYS_shmctl) return 1;
#endif
#ifdef SYS_shmat
    if (number == SYS_shmat) return 1;
#endif
#ifdef SYS_shmdt
    if (number == SYS_shmdt) return 1;
#endif
    /* SYS_io_uring_setup/enter/register (425-427) are blocked for app UIDs. */
    if (number == 425 || number == 426 || number == 427) return 1;
    return 0;
}

static void report_unknown_probe(long number) {
    static const char prefix[] = "BIONICX unknown Android seccomp syscall=";
    char message[sizeof(prefix) + 24];
    size_t length = 0;
    for (size_t index = 0; index < sizeof(prefix) - 1; ++index) {
        message[length++] = prefix[index];
    }
    unsigned long value = number < 0 ? (unsigned long)-number :
                                       (unsigned long)number;
    if (number < 0) message[length++] = '-';
    char digits[24];
    size_t digit_count = 0;
    do {
        digits[digit_count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0 && digit_count < sizeof(digits));
    while (digit_count != 0) message[length++] = digits[--digit_count];
    message[length++] = '\n';
    (void)write(STDERR_FILENO, message, length);
}

static void handle_seccomp_trap(int signal_number, siginfo_t *info,
                                void *raw_context) {
    (void)signal_number;
#if !defined(__aarch64__)
    (void)raw_context;
#endif
    if (is_android_seccomp_probe(info->si_syscall)) {
#if defined(__aarch64__)
        ucontext_t *context = raw_context;
        context->uc_mcontext.regs[0] = (uint64_t)-ENOSYS;
        return;
#endif
    }
    report_unknown_probe(info->si_syscall);
    _exit(128 + SIGSYS);
}

__attribute__((constructor)) static void install_android_kernel_contract(void) {
    struct sigaction action = {0};
    action.sa_sigaction = handle_seccomp_trap;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    (void)sigaction(SIGSYS, &action, NULL);
}
