#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t children[3];

static void stop_children(int signal_number) {
    (void)signal_number;
    for (size_t i = 0; i < 3; ++i) {
        if (children[i] > 0) kill(children[i], SIGTERM);
    }
}

static pid_t start(char *const argv[]) {
    pid_t child = fork();
    if (child == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    return child;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s ICEWM APP1 APP2 [ARGS...]\n", argv[0]);
        return 2;
    }
    signal(SIGTERM, stop_children);
    signal(SIGINT, stop_children);
    char *icewm[] = {argv[1], NULL};
    char *app1[] = {argv[2], NULL};
    char *app2[] = {argv[3], NULL};
    children[0] = start(icewm);
    usleep(400000);
    children[1] = start(app1);
    usleep(200000);
    children[2] = start(app2);
    printf("BXTEST PASS desktop-session-launch icewm=%d app1=%d app2=%d\n",
           (int)children[0], (int)children[1], (int)children[2]);
    fflush(stdout);
    int remaining = 3;
    while (remaining > 0) {
        int status = 0;
        pid_t done = wait(&status);
        if (done < 0) {
            if (errno == EINTR) continue;
            break;
        }
        remaining--;
    }
    printf("BXSUMMARY desktop-session waited=%d\n", 3 - remaining);
    return 0;
}
