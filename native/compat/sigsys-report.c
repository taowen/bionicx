#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <unistd.h>

static char *append_text(char *cursor, const char *text) {
    while (*text != '\0') *cursor++ = *text++;
    return cursor;
}

static char *append_decimal(char *cursor, unsigned int value) {
    char reversed[16];
    unsigned int count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count != 0) *cursor++ = reversed[--count];
    return cursor;
}

static char *append_hex(char *cursor, uintptr_t value) {
    static const char digits[] = "0123456789abcdef";
    int shift = (int)(sizeof(value) * 8) - 4;
    cursor = append_text(cursor, "0x");
    while (shift > 0 && ((value >> shift) & 0xf) == 0) shift -= 4;
    do {
        *cursor++ = digits[(value >> shift) & 0xf];
        shift -= 4;
    } while (shift >= 0);
    return cursor;
}

static void report_sigsys(int signal_number, siginfo_t *info, void *context) {
    (void)signal_number;
    (void)context;
    char message[192];
    char *cursor = append_text(message, "BXSIGSYS syscall=");
    cursor = append_decimal(cursor, (unsigned int)info->si_syscall);
    cursor = append_text(cursor, " arch=");
    cursor = append_hex(cursor, (uintptr_t)info->si_arch);
    cursor = append_text(cursor, " code=");
    cursor = append_decimal(cursor, (unsigned int)info->si_code);
    cursor = append_text(cursor, " address=");
    cursor = append_hex(cursor, (uintptr_t)info->si_call_addr);
    *cursor++ = '\n';
    (void)write(STDERR_FILENO, message, (size_t)(cursor - message));
    _exit(128 + SIGSYS);
}

__attribute__((constructor)) static void install_sigsys_reporter(void) {
    struct sigaction action = {0};
    action.sa_sigaction = report_sigsys;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    (void)sigaction(SIGSYS, &action, NULL);
}
