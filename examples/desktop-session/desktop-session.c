#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t children[4];
static size_t child_count;

static int on_x_error(Display *display, XErrorEvent *event) {
    (void)display;
    (void)event;
    return 0;
}

static void result(const char *name, int passed, const char *detail) {
    printf("BXTEST %s %s%s%s\n", passed ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

static void sleep_ms(int milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static void stop_children(int signal_number) {
    (void)signal_number;
    for (size_t i = 0; i < child_count; ++i) {
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
    if (child > 0 && child_count < 4) children[child_count++] = child;
    return child;
}

static int class_matches(const XClassHint *hint, const char *wanted) {
    if (hint->res_class != NULL && strcasecmp(hint->res_class, wanted) == 0)
        return 1;
    if (hint->res_name != NULL && strcasecmp(hint->res_name, wanted) == 0)
        return 1;
    return 0;
}

static void consider_window(Display *display, Window window, const char *wanted,
                            Window *best, int *best_area) {
    XClassHint hint = {0};
    if (!XGetClassHint(display, window, &hint)) return;
    int match = class_matches(&hint, wanted);
    if (hint.res_name != NULL) XFree(hint.res_name);
    if (hint.res_class != NULL) XFree(hint.res_class);
    if (!match) return;
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)) return;
    if (attributes.map_state != IsViewable || attributes.width < 80 ||
            attributes.height < 40)
        return;
    int area = attributes.width * attributes.height;
    if (*best == None || area < *best_area) {
        *best = window;
        *best_area = area;
    }
}

static void walk_class(Display *display, Window window, const char *wanted,
                       Window *best, int *best_area, int depth, int *visited) {
    if (depth > 5 || *visited > 256) return;
    ++*visited;
    consider_window(display, window, wanted, best, best_area);
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &count))
        return;
    if (count > 64) count = 64;
    for (unsigned i = 0; i < count; ++i)
        walk_class(display, kids[i], wanted, best, best_area, depth + 1,
                   visited);
    if (kids != NULL) XFree(kids);
}

static Window find_class(Display *display, Window root, const char *wanted) {
    Window best = None;
    int best_area = 0;
    int visited = 0;
    walk_class(display, root, wanted, &best, &best_area, 0, &visited);
    return best;
}

static Window wait_class(Display *display, Window root, const char *wanted,
                         int timeout_ms) {
    int waited = 0;
    while (waited <= timeout_ms) {
        Window window = find_class(display, root, wanted);
        if (window != None) return window;
        sleep_ms(100);
        waited += 100;
    }
    return None;
}

static int window_in_tree(Display *display, Window ancestor, Window target,
                          int depth) {
    if (ancestor == None || target == None || depth > 12) return 0;
    if (ancestor == target) return 1;
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, ancestor, &query_root, &parent, &kids, &count))
        return 0;
    int found = 0;
    for (unsigned i = 0; i < count && !found; ++i)
        found = window_in_tree(display, kids[i], target, depth + 1);
    if (kids != NULL) XFree(kids);
    return found;
}

static Window toplevel_frame(Display *display, Window window, Window root) {
    Window current = window;
    for (int i = 0; i < 12; ++i) {
        Window query_root = None;
        Window parent = None;
        Window *kids = NULL;
        unsigned count = 0;
        if (!XQueryTree(display, current, &query_root, &parent, &kids, &count))
            break;
        if (kids != NULL) XFree(kids);
        if (parent == None || parent == root) return current;
        current = parent;
    }
    return window;
}

static int focus_belongs(Display *display, Window client) {
    Window focus = None;
    int revert = 0;
    XGetInputFocus(display, &focus, &revert);
    if (focus == None || focus == PointerRoot) return 0;
    if (focus == client) return 1;
    if (window_in_tree(display, client, focus, 0)) return 1;
    Window frame = toplevel_frame(display, client, DefaultRootWindow(display));
    return focus == frame || window_in_tree(display, frame, focus, 0);
}

static void activate(Display *display, Window client) {
    Window root = DefaultRootWindow(display);
    Window frame = toplevel_frame(display, client, root);
    Atom net_active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.window = client;
    event.xclient.message_type = net_active;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XMapRaised(display, frame);
    XSetInputFocus(display, client, RevertToParent, CurrentTime);
    XSync(display, False);
}

static int wait_focus(Display *display, Window client, int timeout_ms) {
    int waited = 0;
    while (waited <= timeout_ms) {
        if (focus_belongs(display, client)) return 1;
        activate(display, client);
        sleep_ms(100);
        waited += 100;
    }
    return 0;
}

static int send_close(Display *display, Window client) {
    Window root = DefaultRootWindow(display);
    Atom net_close = XInternAtom(display, "_NET_CLOSE_WINDOW", False);
    Atom protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    Atom delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.window = client;
    event.xclient.message_type = net_close;
    event.xclient.format = 32;
    event.xclient.data.l[0] = CurrentTime;
    event.xclient.data.l[1] = 2;
    XSendEvent(display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    event.xclient.message_type = protocols;
    event.xclient.data.l[0] = (long)delete_window;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, client, False, NoEventMask, &event);
    XUnmapWindow(display, client);
    XUnmapWindow(display, toplevel_frame(display, client, root));
    XSync(display, False);
    return 1;
}

static int accept_session(Display *display, char *app1_path, char *app2_path) {
    (void)app1_path;
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[256];

    result("session-display", 1, XDisplayString(display));
    ++passed;
    Window root = DefaultRootWindow(display);

    Window xterm = wait_class(display, root, "XTerm", 8000);
    Window mousepad = wait_class(display, root, "Mousepad", 8000);
    int mapped = xterm != None && mousepad != None;
    snprintf(detail, sizeof(detail), "xterm=0x%lx mousepad=0x%lx",
             (unsigned long)xterm, (unsigned long)mousepad);
    result("session-two-mapped", mapped, detail);
    RECORD(mapped);
    if (!mapped) {
        XCloseDisplay(display);
        printf("BXSUMMARY desktop-session-accept passed=%d failed=%d\n",
               passed, failed);
        return 1;
    }

    activate(display, xterm);
    int xterm_focus = wait_focus(display, xterm, 2000);
    result("session-switch-xterm", xterm_focus,
           xterm_focus ? "focus=xterm" : "focus not xterm");
    RECORD(xterm_focus);

    activate(display, mousepad);
    int mousepad_focus = wait_focus(display, mousepad, 2000);
    result("session-switch-mousepad", mousepad_focus,
           mousepad_focus ? "focus=mousepad" : "focus not mousepad");
    RECORD(mousepad_focus);

    XWindowAttributes before = {0};
    XGetWindowAttributes(display, xterm, &before);
    unsigned new_width = (unsigned)before.width + 96;
    unsigned new_height = (unsigned)before.height + 48;
    XResizeWindow(display, xterm, new_width, new_height);
    XSync(display, False);
    int resized = 0;
    for (int i = 0; i < 20; ++i) {
        XWindowAttributes after = {0};
        XGetWindowAttributes(display, xterm, &after);
        if (after.width != before.width || after.height != before.height) {
            snprintf(detail, sizeof(detail), "%dx%d -> %dx%d",
                     before.width, before.height, after.width, after.height);
            resized = 1;
            break;
        }
        sleep_ms(100);
    }
    result("session-resize-xterm", resized,
           resized ? detail : "geometry unchanged");
    RECORD(resized);

    send_close(display, mousepad);
    int closed = 0;
    for (int i = 0; i < 20; ++i) {
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, mousepad, &attributes) ||
                attributes.map_state != IsViewable) {
            closed = 1;
            break;
        }
        sleep_ms(100);
    }
    if (!closed) {
        XKillClient(display, mousepad);
        XSync(display, False);
        for (int i = 0; i < 20; ++i) {
            XWindowAttributes attributes = {0};
            if (!XGetWindowAttributes(display, mousepad, &attributes) ||
                    attributes.map_state != IsViewable) {
                closed = 1;
                break;
            }
            sleep_ms(100);
        }
    }
    result("session-close-mousepad", closed,
           closed ? "client withdrawn" : "still viewable");
    RECORD(closed);

    int reopened = 0;
    if (closed) {
        sleep_ms(400);
        char *restart[] = {app2_path, "--disable-server", NULL};
        pid_t child = start(restart);
        printf("BXINFO reopen-start pid=%d\n", (int)child);
        fflush(stdout);
        sleep_ms(2000);
        reopened = child > 0 && kill(child, 0) == 0;
        snprintf(detail, sizeof(detail), "pid=%d alive=%d",
                 (int)child, reopened);
    }
    result("session-reopen-mousepad", reopened,
           reopened ? detail : "window did not return");
    RECORD(reopened);

    printf("BXSUMMARY desktop-session-accept passed=%d failed=%d\n",
           passed, failed);
    fflush(stdout);
    return failed == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    int accept = 0;
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--accept") == 0) {
        accept = 1;
        ++argi;
    }
    if (argc - argi < 3) {
        fprintf(stderr, "usage: %s [--accept] ICEWM APP1 APP2\n", argv[0]);
        return 2;
    }
    signal(SIGTERM, stop_children);
    signal(SIGINT, stop_children);
    XSetErrorHandler(on_x_error);
    char *icewm[] = {argv[argi], NULL};
    char *app1[] = {argv[argi + 1], NULL};
    char *app2[] = {argv[argi + 2], NULL};
    start(icewm);
    sleep_ms(400);
    start(app1);
    sleep_ms(200);
    start(app2);
    printf("BXTEST PASS desktop-session-launch icewm=%d app1=%d app2=%d\n",
           (int)children[0], (int)children[1], (int)children[2]);
    fflush(stdout);

    int accept_status = 0;
    if (accept) {
        sleep_ms(1500);
        Display *display = XOpenDisplay(NULL);
        if (display == NULL) {
            result("session-display", 0, "DISPLAY");
            accept_status = 1;
        }
        else {
            int fd = ConnectionNumber(display);
            if (fd >= 0) fcntl(fd, F_SETFD, FD_CLOEXEC);
            accept_status = accept_session(display, argv[argi + 1],
                                           argv[argi + 2]);
            XCloseDisplay(display);
        }
    }

    int remaining = (int)child_count;
    while (remaining > 0) {
        int status = 0;
        pid_t done = wait(&status);
        if (done < 0) {
            if (errno == EINTR) continue;
            break;
        }
        remaining--;
    }
    printf("BXSUMMARY desktop-session waited=%d accept=%d\n",
           (int)child_count - remaining, accept_status);
    return accept ? accept_status : 0;
}
