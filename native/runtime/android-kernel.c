#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#if defined(SYS_accept4)
int accept(int socket_fd, struct sockaddr *address, socklen_t *address_length) {
    return (int)syscall(SYS_accept4, socket_fd, address, address_length, 0);
}
#endif

int pthread_mutexattr_setrobust(pthread_mutexattr_t *attributes,
                                int robustness) {
    static int (*next)(pthread_mutexattr_t *, int);
    if (robustness == PTHREAD_MUTEX_ROBUST) return ENOTSUP;
    if (next == NULL) next = dlsym(RTLD_NEXT, "pthread_mutexattr_setrobust");
    return next(attributes, robustness);
}

#if defined(__aarch64__) && defined(SYS_clone3)
/*
 * glibc blocks signals while pthread_create calls clone3.  Android turns its
 * blocked clone3 syscall into SIGSYS, so a SIGSYS handler cannot provide the
 * ENOSYS result which glibc needs in order to select its clone implementation. Disable
 * only glibc's validated clone3 syscall stub before application constructors
 * start.  The replacement sets x0 to -ENOSYS; the existing following error
 * branch sets errno and preserves glibc's normal selection semantics.
 */
struct patch_counts {
    int clone3;
    int robust_list;
    int failed;
};

static int replace_syscall(uint32_t *instruction, uint32_t replacement,
                           long page_size) {
    uintptr_t page = (uintptr_t)instruction & ~((uintptr_t)page_size - 1u);
    if (mprotect((void *)page, (size_t)page_size,
                 PROT_READ | PROT_WRITE) != 0) {
        return -1;
    }
    *instruction = replacement;
    __builtin___clear_cache((char *)instruction, (char *)(instruction + 1));
    if (mprotect((void *)page, (size_t)page_size,
                 PROT_READ | PROT_EXEC) != 0) {
        return -1;
    }
    return 0;
}

static int disable_blocked_glibc_syscalls(struct dl_phdr_info *info,
                                          size_t size, void *raw_counts) {
    (void)size;
    if (info->dlpi_name == NULL ||
        strstr(info->dlpi_name, "/libc.so.6") == NULL) {
        return 0;
    }

    struct patch_counts *counts = raw_counts;
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) return 1;
    for (ElfW(Half) index = 0; index < info->dlpi_phnum; ++index) {
        const ElfW(Phdr) *segment = &info->dlpi_phdr[index];
        if (segment->p_type != PT_LOAD || (segment->p_flags & PF_X) == 0) {
            continue;
        }
        uint32_t *cursor = (uint32_t *)(info->dlpi_addr + segment->p_vaddr);
        uint32_t *end = (uint32_t *)((char *)cursor + segment->p_memsz);
        for (; cursor + 2 < end; ++cursor) {
            /* mov x8,#435; svc #0; cmp x0,#0 */
            if (cursor[0] != 0xd2803668u || cursor[1] != 0xd4000001u ||
                cursor[2] != 0xf100001fu) {
                continue;
            }
            /* mov x0,#-ENOSYS */
            if (replace_syscall(&cursor[1], 0x928004a0u, page_size) != 0) {
                ++counts->failed;
            }
            else ++counts->clone3;
        }
        for (cursor = (uint32_t *)(info->dlpi_addr + segment->p_vaddr);
             cursor + 3 < end; ++cursor) {
            /* set_robust_list is issued while new pthread signals are blocked. */
            if (cursor[0] != 0xd2800c68u) continue; /* mov x8,#99 */
            for (int distance = 1; distance <= 3; ++distance) {
                if (cursor[distance] != 0xd4000001u) continue; /* svc #0 */
                /* glibc ignores this registration result. Public robust
                 * attributes are rejected below, so no robust state is promised. */
                if (replace_syscall(&cursor[distance], 0xd2800000u,
                                    page_size) != 0) {
                    ++counts->failed;
                } else {
                    ++counts->robust_list;
                }
                break;
            }
        }
    }
    return 1;
}

static void adapt_blocked_glibc_syscalls(void) {
    struct patch_counts counts = {0};
    (void)dl_iterate_phdr(disable_blocked_glibc_syscalls, &counts);
    if (counts.clone3 != 1 || counts.robust_list == 0 || counts.failed != 0) {
        static const char message[] =
            "BIONICX could not adapt blocked glibc syscall stubs\n";
        (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    }
}
#else
static void adapt_blocked_glibc_syscalls(void) {}
#endif

static int is_optional_probe(long number) {
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
    if (is_optional_probe(info->si_syscall)) {
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
    adapt_blocked_glibc_syscalls();
}
