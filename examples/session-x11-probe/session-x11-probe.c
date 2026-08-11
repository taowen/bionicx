#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void sleep_milliseconds(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000 * 1000,
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static void draw(Display *display, Window window, GC graphics,
                 int screen, int width, int height) {
    XSetForeground(display, graphics, 0x18243a);
    XFillRectangle(display, window, graphics, 0, 0,
                   (unsigned int)width, (unsigned int)height);
    XSetForeground(display, graphics, 0x35c98b);
    XFillRectangle(display, window, graphics, 44, 46,
                   (unsigned int)(width - 88), 14);
    XSetForeground(display, graphics, WhitePixel(display, screen));
    const char *title = "BionicX supervised glibc session";
    const char *detail = "primary exited; detached X11 grandchild remains";
    XDrawString(display, window, graphics, 54, 130, title,
                (int)strlen(title));
    XDrawString(display, window, graphics, 54, 176, detail,
                (int)strlen(detail));
    XFlush(display);
}

static int run_worker(int duration_seconds) {
    prctl(PR_SET_NAME, "bx-session-x11", 0, 0, 0);
    sleep_milliseconds(600);

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXSESSION FAIL detached-display display=%s\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "<unset>");
        return 10;
    }
    int screen = DefaultScreen(display);
    int width = 920;
    int height = 430;
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
            120, 140, (unsigned int)width, (unsigned int)height, 0,
            BlackPixel(display, screen), 0x18243a);
    XStoreName(display, window, "BionicX session X11 probe");
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);
    GC graphics = XCreateGC(display, window, 0, NULL);

    printf("BXSESSION detached pid=%ld ppid=%ld sid=%ld\n", (long)getpid(),
           (long)getppid(), (long)getsid(0));
    fflush(stdout);
    draw(display, window, graphics, screen, width, height);

    for (int tick = 0; tick < duration_seconds * 10; ++tick) {
        while (XPending(display) != 0) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == Expose)
                draw(display, window, graphics, screen, width, height);
        }
        sleep_milliseconds(100);
    }

    printf("BXTEST PASS session-x11 detached-client duration=%d\n",
           duration_seconds);
    printf("BXSUMMARY session-x11 passed=3/3\n");
    fflush(stdout);
    XFreeGC(display, graphics);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}

int main(int argc, char **argv) {
    int duration_seconds = argc > 1 ? atoi(argv[1]) : 8;
    if (duration_seconds < 2 || duration_seconds > 120) {
        fprintf(stderr, "duration must be between 2 and 120 seconds\n");
        return 2;
    }

    pid_t intermediate = fork();
    if (intermediate < 0) {
        perror("fork intermediate");
        return 3;
    }
    if (intermediate == 0) {
        if (setsid() < 0) _exit(20);
        pid_t worker = fork();
        if (worker < 0) _exit(21);
        if (worker != 0) _exit(0);
        _exit(run_worker(duration_seconds));
    }

    int status = 0;
    while (waitpid(intermediate, &status, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "BXSESSION FAIL intermediate status=0x%x\n", status);
        return 4;
    }
    printf("BXTEST PASS session-x11 primary-exit pid=%ld detached-via=%ld\n",
           (long)getpid(), (long)intermediate);
    printf("BXTEST PASS session-x11 detached-session-created\n");
    fflush(stdout);
    return 0;
}
