#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void sleep_milliseconds(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = milliseconds % 1000 * 1000 * 1000,
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static pid_t launch(const char* executable, char* const arguments[]) {
    pid_t child = fork();
    if (child != 0) return child;
    execv(executable, arguments);
    _exit(126);
}

static int wait_success(pid_t child) {
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void) {
    const char* app = getenv("BIONICX_APP");
    const char* runtime = getenv("BIONICX_RUNTIME");
    if (app == NULL || runtime == NULL) return 2;
    char icewm[1024];
    char client[1024];
    snprintf(icewm, sizeof(icewm), "%s/usr/bin/icewm", runtime);
    snprintf(client, sizeof(client), "%s/bin/icewm-window", app);

    char* wm_arguments[] = {icewm, "--sync", "--trace=conf,prog", NULL};
    pid_t wm = launch(icewm, wm_arguments);
    if (wm < 0) return 3;
    sleep_milliseconds(1200);
    int wm_status = 0;
    pid_t wm_wait = waitpid(wm, &wm_status, WNOHANG);
    if (wm_wait == wm || kill(wm, 0) != 0) {
        fprintf(stderr, "BXICEWM FAIL manager-start status=0x%x\n",
                wm_status);
        return 4;
    }
    printf("BXTEST PASS icewm-manager-start pid=%ld\n", (long)wm);
    fflush(stdout);

    char* taskbar_arguments[] = {client, "--check-taskbar", NULL};
    pid_t taskbar = launch(client, taskbar_arguments);
    int taskbar_ok = taskbar > 0 && wait_success(taskbar);

    char* first_arguments[] = {
        client, "BionicX Workspace A", "120", "130", "0x285078", "20", NULL,
    };
    char* second_arguments[] = {
        client, "BionicX Workspace B", "760", "300", "0x684080", "20", NULL,
    };
    pid_t first = launch(client, first_arguments);
    pid_t second = launch(client, second_arguments);
    sleep_milliseconds(1500);
    char* tree_arguments[] = {client, "--dump-tree", NULL};
    pid_t tree = launch(client, tree_arguments);
    int tree_ok = tree > 0 && wait_success(tree);
    printf("BXTEST %s icewm-window-tree\n", tree_ok ? "PASS" : "FAIL");
    fflush(stdout);
    int first_ok = first > 0 && wait_success(first);
    int second_ok = second > 0 && wait_success(second);
    kill(wm, SIGTERM);
    waitpid(wm, NULL, 0);

    printf("BXTEST %s icewm-two-clients first=%d second=%d\n",
           first_ok && second_ok ? "PASS" : "FAIL", first_ok, second_ok);
    int passed = 1 + taskbar_ok + tree_ok + (first_ok && second_ok);
    int failed = 4 - passed;
    printf("BXSUMMARY icewm passed=%d failed=%d\n", passed, failed);
    fflush(stdout);
    return taskbar_ok && tree_ok && first_ok && second_ok ? 0 : 5;
}
