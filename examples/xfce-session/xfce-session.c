#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t children[12];
static size_t child_count;
static FILE *accept_log;
static char gdk_ev_log[512];

static void accept_log_line(const char *fmt, ...) {
    if (accept_log == NULL) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(accept_log, fmt, ap);
    va_end(ap);
    fflush(accept_log);
}

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

static int exists_exec(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IXUSR);
}

static pid_t start(char *const argv[]) {
    if (argv[0] == NULL || !exists_exec(argv[0])) {
        printf("BXINFO skip missing %s\n", argv[0] ? argv[0] : "(null)");
        fflush(stdout);
        return -1;
    }
    pid_t child = fork();
    if (child == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    if (child > 0 && child_count < 12) children[child_count++] = child;
    return child;
}

static pid_t start_logged(char *const argv[], const char *log_path,
                          const char *gdk_debug) {
    if (argv[0] == NULL || !exists_exec(argv[0])) {
        printf("BXINFO skip missing %s\n", argv[0] ? argv[0] : "(null)");
        fflush(stdout);
        return -1;
    }
    pid_t child = fork();
    if (child == 0) {
        if (gdk_debug != NULL) setenv("GDK_DEBUG", gdk_debug, 1);
        if (log_path != NULL) {
            int fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    if (child > 0 && child_count < 12) children[child_count++] = child;
    return child;
}

static long file_size_of(const char *path) {
    struct stat info;
    if (path == NULL || stat(path, &info) != 0) return 0;
    return (long)info.st_size;
}

static int gdk_ev_interesting(const char *line) {
    return strstr(line, "button") != NULL
            || strstr(line, "Button") != NULL
            || strstr(line, "generic") != NULL
            || strstr(line, "Generic") != NULL
            || strstr(line, "filter") != NULL
            || strstr(line, "XI") != NULL;
}

static void dump_gdk_ev(const char *path, long from) {
    FILE *log = fopen(path, "r");
    if (log == NULL) {
        printf("BXINFO gdk-ev missing path=%s\n", path ? path : "(null)");
        fflush(stdout);
        return;
    }
    if (from > 0) fseek(log, from, SEEK_SET);
    char line[512];
    int matched = 0;
    int printed = 0;
    int new_lines = 0;
    while (fgets(line, sizeof(line), log) != NULL) {
        ++new_lines;
        if (!gdk_ev_interesting(line)) continue;
        ++matched;
        if (printed >= 32) continue;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        printf("BXINFO gdk-ev %s\n", line);
        ++printed;
    }
    fclose(log);
    printf("BXINFO gdk-ev-summary matched=%d printed=%d new_lines=%d from=%ld\n",
           matched, printed, new_lines, from);
    fflush(stdout);
}

static int class_matches(const XClassHint *hint, const char *wanted) {
    if (hint->res_class != NULL && strcasecmp(hint->res_class, wanted) == 0)
        return 1;
    if (hint->res_name != NULL && strcasecmp(hint->res_name, wanted) == 0)
        return 1;
    return 0;
}

static void consider_window(Display *display, Window window, const char *wanted,
                            Window *best, int *best_area, int require_viewable) {
    XClassHint hint = {0};
    if (!XGetClassHint(display, window, &hint)) return;
    int match = class_matches(&hint, wanted);
    if (hint.res_name != NULL) XFree(hint.res_name);
    if (hint.res_class != NULL) XFree(hint.res_class);
    if (!match) return;
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)) return;
    if (attributes.width < 8 || attributes.height < 4) return;
    if (require_viewable && attributes.map_state != IsViewable) return;
    int area = attributes.width * attributes.height;
    int better = *best == None;
    if (!better && require_viewable) better = area < *best_area;
    if (!better && !require_viewable) better = area > *best_area;
    if (better) {
        *best = window;
        *best_area = area;
    }
}

static void walk_class(Display *display, Window window, const char *wanted,
                       Window *best, int *best_area, int depth, int *visited,
                       int require_viewable) {
    if (depth > 8 || *visited > 400) return;
    ++*visited;
    consider_window(display, window, wanted, best, best_area, require_viewable);
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &count))
        return;
    for (unsigned i = 0; i < count; ++i)
        walk_class(display, kids[i], wanted, best, best_area, depth + 1,
                   visited, require_viewable);
    if (kids != NULL) XFree(kids);
}

static void dump_mapped_walk(Display *display, Window window, int depth,
                             int *visited) {
    if (depth > 8 || *visited > 400) return;
    ++*visited;
    XClassHint hint = {0};
    XWindowAttributes attributes = {0};
    if (XGetWindowAttributes(display, window, &attributes)) {
        char name[64] = "-";
        char klass[64] = "-";
        if (XGetClassHint(display, window, &hint)) {
            if (hint.res_name != NULL) {
                snprintf(name, sizeof(name), "%s", hint.res_name);
                XFree(hint.res_name);
            }
            if (hint.res_class != NULL) {
                snprintf(klass, sizeof(klass), "%s", hint.res_class);
                XFree(hint.res_class);
            }
        }
        if (attributes.map_state == IsViewable || klass[0] != '-') {
            Window query_root = None;
            Window parent = None;
            Window *kids = NULL;
            unsigned count = 0;
            if (XQueryTree(display, window, &query_root, &parent, &kids,
                           &count) && kids != NULL)
                XFree(kids);
            printf("BXINFO win 0x%lx parent=0x%lx map=%d override=%d "
                   "%dx%d+%d+%d name=%s class=%s\n",
                   (unsigned long)window, (unsigned long)parent,
                   attributes.map_state, attributes.override_redirect,
                   attributes.width, attributes.height, attributes.x,
                   attributes.y, name, klass);
            fflush(stdout);
        }
    }
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &count))
        return;
    for (unsigned i = 0; i < count; ++i)
        dump_mapped_walk(display, kids[i], depth + 1, visited);
    if (kids != NULL) XFree(kids);
}

static void dump_mapped(Display *display, Window root) {
    int visited = 0;
    dump_mapped_walk(display, root, 0, &visited);
}

static Window find_class(Display *display, Window root, const char *wanted) {
    Window best = None;
    int best_area = 0;
    int visited = 0;
    walk_class(display, root, wanted, &best, &best_area, 0, &visited, 1);
    return best;
}

static void walk_widest(Display *display, Window window, const char *wanted,
                        Window *best, int *best_width, int depth, int *visited) {
    if (depth > 8 || *visited > 400) return;
    ++*visited;
    XClassHint hint = {0};
    if (XGetClassHint(display, window, &hint)) {
        int match = class_matches(&hint, wanted);
        if (hint.res_name != NULL) XFree(hint.res_name);
        if (hint.res_class != NULL) XFree(hint.res_class);
        if (match) {
            XWindowAttributes attributes = {0};
            if (XGetWindowAttributes(display, window, &attributes)
                    && attributes.map_state == IsViewable
                    && attributes.width > *best_width) {
                *best = window;
                *best_width = attributes.width;
            }
        }
    }
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &count))
        return;
    for (unsigned i = 0; i < count; ++i)
        walk_widest(display, kids[i], wanted, best, best_width, depth + 1,
                    visited);
    if (kids != NULL) XFree(kids);
}

static Window find_widest_class(Display *display, Window root,
                                const char *wanted) {
    Window best = None;
    int best_width = 0;
    int visited = 0;
    walk_widest(display, root, wanted, &best, &best_width, 0, &visited);
    return best;
}

static int window_listed(const Window *windows, int count, Window window) {
    for (int i = 0; i < count; ++i) {
        if (windows[i] == window) return 1;
    }
    return 0;
}

static int is_screen_sized(Display *display, const XWindowAttributes *attributes) {
    int screen = DefaultScreen(display);
    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    return attributes->width >= width - 16
            && attributes->height >= height - 16;
}

static int collect_override(Display *display, Window root, Window *out,
                            int max, int menus_only) {
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    int found = 0;
    if (!XQueryTree(display, root, &query_root, &parent, &kids, &count)
            || kids == NULL)
        return 0;
    for (unsigned i = 0; i < count && found < max; ++i) {
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, kids[i], &attributes)) continue;
        if (attributes.map_state != IsViewable || !attributes.override_redirect)
            continue;
        if (menus_only) {
            if (is_screen_sized(display, &attributes)) continue;
            if (attributes.width < 80 || attributes.height < 80) continue;
        }
        out[found++] = kids[i];
    }
    XFree(kids);
    return found;
}

static int wait_new_menu(Display *display, Window root, const Window *before,
                         int before_n, int timeout_ms, char *detail,
                         size_t detail_size) {
    int waited = 0;
    while (waited <= timeout_ms) {
        Window after[32];
        int after_n = collect_override(display, root, after, 32, 1);
        for (int k = 0; k < after_n; ++k) {
            if (window_listed(before, before_n, after[k])) continue;
            XWindowAttributes attributes = {0};
            if (!XGetWindowAttributes(display, after[k], &attributes))
                continue;
            snprintf(detail, detail_size, "menu %dx%d", attributes.width,
                     attributes.height);
            sleep_ms(400);
            XWindowAttributes painted = {0};
            if (XGetWindowAttributes(display, after[k], &painted))
                attributes = painted;
            int sample_x = attributes.width > 80 ? attributes.width / 2 : 8;
            int sample_y = attributes.height > 80 ? attributes.height / 2 : 8;
            XImage *image = XGetImage(display, after[k], sample_x, sample_y,
                                      1, 1, AllPlanes, ZPixmap);
            unsigned long pixel = image != NULL ? XGetPixel(image, 0, 0) : 0;
            if (image != NULL) XDestroyImage(image);
            int nonzero = 0;
            int light = 0;
            unsigned long brightest = pixel & 0xffffff;
            XImage *full = attributes.width > 0 && attributes.height > 0
                    ? XGetImage(display, after[k], 0, 0,
                                (unsigned)attributes.width,
                                (unsigned)attributes.height,
                                AllPlanes, ZPixmap)
                    : NULL;
            if (full != NULL) {
                for (int y = 0; y < attributes.height; y++) {
                    for (int x = 0; x < attributes.width; x++) {
                        unsigned long p = XGetPixel(full, x, y) & 0xffffff;
                        if (p != 0) ++nonzero;
                        int red = (int)((p >> 16) & 0xff);
                        int green = (int)((p >> 8) & 0xff);
                        int blue = (int)(p & 0xff);
                        if (red + green + blue >= 0x180) ++light;
                        if (p > brightest) brightest = p;
                    }
                }
                XDestroyImage(full);
            }
            printf("BXINFO menu-paint 0x%lx %dx%d+%d+%d depth=%d pixel=0x%06lx nonzero=%d light=%d bright=0x%06lx\n",
                   (unsigned long)after[k], attributes.width, attributes.height,
                   attributes.x, attributes.y, attributes.depth,
                   pixel & 0xffffff, nonzero, light, brightest);
            Window menu_root = None;
            Window menu_parent = None;
            Window *menu_kids = NULL;
            unsigned menu_count = 0;
            if (XQueryTree(display, after[k], &menu_root, &menu_parent,
                           &menu_kids, &menu_count) && menu_kids != NULL) {
                for (unsigned c = 0; c < menu_count && c < 6; ++c) {
                    XWindowAttributes child_attr = {0};
                    if (!XGetWindowAttributes(display, menu_kids[c],
                                              &child_attr))
                        continue;
                    XImage *child_image = child_attr.width > 4
                            && child_attr.height > 4
                            ? XGetImage(display, menu_kids[c], 2, 2, 1, 1,
                                        AllPlanes, ZPixmap) : NULL;
                    unsigned long child_pixel = child_image != NULL
                            ? XGetPixel(child_image, 0, 0) : 0;
                    if (child_image != NULL) XDestroyImage(child_image);
                    printf("BXINFO menu-child 0x%lx %dx%d+%d+%d map=%d pixel=0x%06lx\n",
                           (unsigned long)menu_kids[c], child_attr.width,
                           child_attr.height, child_attr.x, child_attr.y,
                           child_attr.map_state, child_pixel & 0xffffff);
                }
                XFree(menu_kids);
            }
            fflush(stdout);
            return 1;
        }
        sleep_ms(100);
        waited += 100;
    }
    return 0;
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

static int wm_is_xfwm(Display *display, Window root) {
    Atom check = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", True);
    if (check == None) return find_class(display, root, "Xfwm4") != None;
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long bytes = 0;
    unsigned char *prop = NULL;
    if (XGetWindowProperty(display, root, check, 0, 1, False, XA_WINDOW,
                           &actual, &format, &nitems, &bytes, &prop)
            != Success || prop == NULL || nitems < 1) {
        if (prop != NULL) XFree(prop);
        return find_class(display, root, "Xfwm4") != None;
    }
    Window wm_win = *(Window *)prop;
    XFree(prop);
    XClassHint hint = {0};
    if (XGetClassHint(display, wm_win, &hint)) {
        int ok = class_matches(&hint, "Xfwm4");
        if (hint.res_name != NULL) XFree(hint.res_name);
        if (hint.res_class != NULL) XFree(hint.res_class);
        if (ok) return 1;
    }
    return find_class(display, root, "Xfwm4") != None;
}

static int compositor_selection_owned(Display *display) {
    char name[32];
    snprintf(name, sizeof(name), "_NET_WM_CM_S%d", DefaultScreen(display));
    Atom cm = XInternAtom(display, name, True);
    if (cm == None) return 0;
    return XGetSelectionOwner(display, cm) != None;
}

static int named_selection_owned(Display *display, const char *name) {
    Atom atom = XInternAtom(display, name, True);
    if (atom == None) return 0;
    return XGetSelectionOwner(display, atom) != None;
}

static void sample_paint(Display *display, Window window, const char *tag) {
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)) {
        printf("BXINFO %s 0x%lx gone\n", tag, (unsigned long)window);
        fflush(stdout);
        return;
    }
    int width = attributes.width;
    int height = attributes.height;
    if (attributes.map_state != IsViewable || width < 2 || height < 2) {
        printf("BXINFO %s 0x%lx %dx%d+%d+%d map=%d skip\n", tag,
               (unsigned long)window, width, height, attributes.x,
               attributes.y, attributes.map_state);
        fflush(stdout);
        return;
    }
    int crop_w = width > 200 ? 200 : width;
    int crop_h = height > 120 ? 120 : height;
    int crop_x = (width - crop_w) / 2;
    int crop_y = (height - crop_h) / 2;
    XImage *image = XGetImage(display, window, crop_x, crop_y,
                              (unsigned)crop_w, (unsigned)crop_h,
                              AllPlanes, ZPixmap);
    unsigned long pixel = 0;
    int nonzero = 0;
    int light = 0;
    int dark = 0;
    if (image != NULL) {
        pixel = XGetPixel(image, crop_w / 2, crop_h / 2) & 0xffffff;
        for (int y = 0; y < crop_h; ++y) {
            for (int x = 0; x < crop_w; ++x) {
                unsigned long p = XGetPixel(image, x, y) & 0xffffff;
                if (p != 0) ++nonzero;
                int red = (int)((p >> 16) & 0xff);
                int green = (int)((p >> 8) & 0xff);
                int blue = (int)(p & 0xff);
                int lum = red + green + blue;
                if (lum >= 0x180) ++light;
                if (lum <= 0x40) ++dark;
            }
        }
        XDestroyImage(image);
    }
    printf("BXINFO %s 0x%lx %dx%d+%d+%d map=%d depth=%d pixel=0x%06lx "
           "nonzero=%d light=%d dark=%d crop=%dx%d\n",
           tag, (unsigned long)window, width, height, attributes.x,
           attributes.y, attributes.map_state, attributes.depth, pixel,
           nonzero, light, dark, crop_w, crop_h);
    fflush(stdout);
}

static void dump_or_paint(Display *display, Window root, const char *tag) {
    Window windows[32];
    int n = collect_override(display, root, windows, 32, 0);
    int painted = 0;
    for (int i = 0; i < n; ++i) {
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, windows[i], &attributes))
            continue;
        if (is_screen_sized(display, &attributes)) continue;
        if (attributes.width >= 80 && attributes.height >= 80) continue;
        if (attributes.width < 20 || attributes.height < 12) continue;
        sample_paint(display, windows[i], tag);
        ++painted;
    }
    if (painted == 0) printf("BXINFO %s none\n", tag);
    fflush(stdout);
}

static void sample_paint_tl(Display *display, Window window, const char *tag) {
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)
            || attributes.map_state != IsViewable
            || attributes.width < 8 || attributes.height < 8) {
        return;
    }
    int crop_w = attributes.width > 120 ? 120 : attributes.width;
    int crop_h = attributes.height > 48 ? 48 : attributes.height;
    XImage *image = XGetImage(display, window, 0, 0, (unsigned)crop_w,
                              (unsigned)crop_h, AllPlanes, ZPixmap);
    unsigned long pixel = 0;
    int nonzero = 0;
    int light = 0;
    int dark = 0;
    int mid = 0;
    if (image != NULL) {
        pixel = XGetPixel(image, 2, 2) & 0xffffff;
        for (int y = 0; y < crop_h; ++y) {
            for (int x = 0; x < crop_w; ++x) {
                unsigned long p = XGetPixel(image, x, y) & 0xffffff;
                if (p != 0) ++nonzero;
                int lum = (int)(((p >> 16) & 0xff) + ((p >> 8) & 0xff)
                        + (p & 0xff));
                if (lum >= 0x180) ++light;
                else if (lum <= 0x40) ++dark;
                else ++mid;
            }
        }
        XDestroyImage(image);
    }
    printf("BXINFO %s 0x%lx %dx%d+%d+%d map=%d depth=%d pixel=0x%06lx "
           "nonzero=%d light=%d mid=%d dark=%d crop=%dx%d\n",
           tag, (unsigned long)window, attributes.width, attributes.height,
           attributes.x, attributes.y, attributes.map_state, attributes.depth,
           pixel, nonzero, light, mid, dark, crop_w, crop_h);
    fflush(stdout);
}

static void dump_class_paint_walk(Display *display, Window window,
                                  const char *tag, int depth, int *count) {
    if (depth > 6 || *count > 24) return;
    XWindowAttributes attributes = {0};
    if (XGetWindowAttributes(display, window, &attributes)
            && attributes.map_state == IsViewable
            && attributes.width >= 8 && attributes.height >= 8) {
        sample_paint_tl(display, window, tag);
        ++*count;
    }
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned n = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &n)
            || kids == NULL)
        return;
    for (unsigned i = 0; i < n && *count <= 24; ++i)
        dump_class_paint_walk(display, kids[i], tag, depth + 1, count);
    XFree(kids);
}

static void sample_paint_body(Display *display, Window window, const char *tag) {
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)
            || attributes.map_state != IsViewable
            || attributes.width < 32 || attributes.height < 80)
        return;
    int crop_w = attributes.width > 160 ? 160 : attributes.width;
    int crop_h = 80;
    int crop_x = 8;
    int crop_y = attributes.height > 120 ? 36 : 24;
    if (crop_x + crop_w > attributes.width)
        crop_w = attributes.width - crop_x;
    if (crop_y + crop_h > attributes.height)
        crop_h = attributes.height - crop_y;
    if (crop_w < 16 || crop_h < 16) return;
    XImage *image = XGetImage(display, window, crop_x, crop_y,
                              (unsigned)crop_w, (unsigned)crop_h,
                              AllPlanes, ZPixmap);
    unsigned long pixel = 0;
    int light = 0;
    int mid = 0;
    int dark = 0;
    if (image != NULL) {
        pixel = XGetPixel(image, 2, 2) & 0xffffff;
        for (int y = 0; y < crop_h; ++y) {
            for (int x = 0; x < crop_w; ++x) {
                unsigned long p = XGetPixel(image, x, y) & 0xffffff;
                int lum = (int)(((p >> 16) & 0xff) + ((p >> 8) & 0xff)
                        + (p & 0xff));
                if (lum >= 0x180) ++light;
                else if (lum <= 0x40) ++dark;
                else ++mid;
            }
        }
        XDestroyImage(image);
    }
    printf("BXINFO %s-body 0x%lx %dx%d+%d+%d pixel=0x%06lx light=%d mid=%d "
           "dark=%d crop=%dx%d+%d+%d\n",
           tag, (unsigned long)window, attributes.width, attributes.height,
           attributes.x, attributes.y, pixel, light, mid, dark,
           crop_w, crop_h, crop_x, crop_y);
    fflush(stdout);
}

static void dump_class_paint(Display *display, Window root, const char *wanted,
                             const char *tag) {
    Window window = find_class(display, root, wanted);
    if (window == None) {
        printf("BXINFO %s none class=%s\n", tag, wanted);
        fflush(stdout);
        return;
    }
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned n = 0;
    if (XQueryTree(display, window, &query_root, &parent, &kids, &n)) {
        printf("BXINFO %s-tree 0x%lx children=%u\n", tag,
               (unsigned long)window, n);
        fflush(stdout);
        for (unsigned i = 0; i < n; ++i) {
            XWindowAttributes child_attr = {0};
            if (!XGetWindowAttributes(display, kids[i], &child_attr)) {
                printf("BXINFO %s-child 0x%lx gone\n", tag,
                       (unsigned long)kids[i]);
                fflush(stdout);
                continue;
            }
            printf("BXINFO %s-child 0x%lx %dx%d+%d+%d map=%d class=%d "
                   "depth=%d mask=0x%lx\n",
                   tag, (unsigned long)kids[i], child_attr.width,
                   child_attr.height, child_attr.x, child_attr.y,
                   child_attr.map_state, child_attr.class, child_attr.depth,
                   (unsigned long)child_attr.your_event_mask);
            fflush(stdout);
            if (child_attr.map_state == IsViewable
                    && child_attr.width >= 16 && child_attr.height >= 16)
                sample_paint_tl(display, kids[i], tag);
        }
        if (kids != NULL) XFree(kids);
    }
    int count = 0;
    dump_class_paint_walk(display, window, tag, 0, &count);
    sample_paint_body(display, window, tag);
    if (count == 0) {
        printf("BXINFO %s empty 0x%lx\n", tag, (unsigned long)window);
        fflush(stdout);
    }
}

static void dump_typed_text(Display *display, Window root) {
    Window mousepad = find_class(display, root, "Mousepad");
    if (mousepad == None) {
        printf("BXINFO mp-type none\n");
        fflush(stdout);
        return;
    }
    activate(display, mousepad);
    sleep_ms(300);
    KeyCode b = XKeysymToKeycode(display, XK_b);
    KeyCode x = XKeysymToKeycode(display, XK_x);
    if (b != 0) {
        XTestFakeKeyEvent(display, b, True, 0);
        XTestFakeKeyEvent(display, b, False, 20);
    }
    if (x != 0) {
        XTestFakeKeyEvent(display, x, True, 0);
        XTestFakeKeyEvent(display, x, False, 20);
    }
    XSync(display, False);
    sleep_ms(400);
    printf("BXINFO mp-type keys b=%u x=%u window=0x%lx\n",
           (unsigned)b, (unsigned)x, (unsigned long)mousepad);
    fflush(stdout);
    dump_class_paint(display, root, "Mousepad", "mp-type");
}

static void dump_override(Display *display, Window root, const char *tag) {
    Window windows[32];
    int n = collect_override(display, root, windows, 32, 0);
    for (int i = 0; i < n; ++i) {
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, windows[i], &attributes))
            continue;
        printf("BXINFO %s 0x%lx %dx%d+%d+%d\n", tag,
               (unsigned long)windows[i], attributes.width, attributes.height,
               attributes.x, attributes.y);
    }
    if (n == 0) printf("BXINFO %s none\n", tag);
    fflush(stdout);
}

static void dump_children(Display *display, Window window, const char *tag) {
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &count)
            || kids == NULL) {
        printf("BXINFO %s none\n", tag);
        fflush(stdout);
        return;
    }
    for (unsigned i = 0; i < count && i < 12; ++i) {
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, kids[i], &attributes)) continue;
        printf("BXINFO %s 0x%lx %dx%d+%d+%d map=%d\n", tag,
               (unsigned long)kids[i], attributes.width, attributes.height,
               attributes.x, attributes.y, attributes.map_state);
    }
    if (count == 0) printf("BXINFO %s none\n", tag);
    XFree(kids);
    fflush(stdout);
}

static int grab_free(Display *display, Window root) {
    int status = XGrabPointer(display, root, False, ButtonPressMask,
                              GrabModeAsync, GrabModeAsync, None, None,
                              CurrentTime);
    if (status == GrabSuccess) XUngrabPointer(display, CurrentTime);
    XSync(display, False);
    return status;
}

static void dump_pointer(Display *display, Window root, const char *tag) {
    Window qroot = None;
    Window qchild = None;
    int qx = 0, qy = 0, wx = 0, wy = 0;
    unsigned qmask = 0;
    XQueryPointer(display, root, &qroot, &qchild, &qx, &qy, &wx, &wy, &qmask);
    XWindowAttributes attributes = {0};
    const char *cls = "none";
    XClassHint hint = {0};
    Window frame_child = None;
    if (qchild != None) {
        XGetWindowAttributes(display, qchild, &attributes);
        if (XGetClassHint(display, qchild, &hint)) {
            cls = hint.res_class != NULL ? hint.res_class
                    : (hint.res_name != NULL ? hint.res_name : "none");
        }
        XQueryPointer(display, qchild, &qroot, &frame_child, &qx, &qy, &wx,
                      &wy, &qmask);
    }
    const char *child_cls = "none";
    XClassHint child_hint = {0};
    XWindowAttributes child_attr = {0};
    if (frame_child != None) {
        XGetWindowAttributes(display, frame_child, &child_attr);
        if (XGetClassHint(display, frame_child, &child_hint)) {
            child_cls = child_hint.res_class != NULL ? child_hint.res_class
                    : (child_hint.res_name != NULL ? child_hint.res_name
                            : "none");
        }
    }
    printf("BXINFO %s child=0x%lx %dx%d+%d+%d class=%s frame_child=0x%lx "
           "%dx%d+%d+%d child_class=%s xy=%d,%d mask=0x%x\n",
           tag, (unsigned long)qchild, attributes.width, attributes.height,
           attributes.x, attributes.y, cls, (unsigned long)frame_child,
           child_attr.width, child_attr.height, child_attr.x, child_attr.y,
           child_cls, qx, qy, qmask);
    accept_log_line("%s child=0x%lx %dx%d+%d+%d class=%s child_class=%s\n",
                    tag, (unsigned long)qchild, attributes.width,
                    attributes.height, attributes.x, attributes.y, cls,
                    child_cls);
    if (child_hint.res_name != NULL) XFree(child_hint.res_name);
    if (child_hint.res_class != NULL) XFree(child_hint.res_class);
    if (hint.res_name != NULL) XFree(hint.res_name);
    if (hint.res_class != NULL) XFree(hint.res_class);
    fflush(stdout);
}

static void dump_window_type(Display *display, Window window, char *out,
                             size_t size) {
    Atom net_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
    if (net_type == None) {
        snprintf(out, size, "-");
        return;
    }
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0, after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(display, window, net_type, 0, 8, False, XA_ATOM,
                           &actual, &format, &nitems, &after, &data) != Success
            || data == NULL || nitems == 0) {
        snprintf(out, size, "-");
        if (data != NULL) XFree(data);
        return;
    }
    char *name = XGetAtomName(display, ((Atom *)data)[0]);
    snprintf(out, size, "%s", name != NULL ? name : "?");
    if (name != NULL) XFree(name);
    XFree(data);
}

static void dump_find_class(Display *display, Window window, Window root,
                            const char *wanted, int depth) {
    if (depth > 10) return;
    XClassHint hint = {0};
    if (XGetClassHint(display, window, &hint)) {
        if (class_matches(&hint, wanted)) {
            XWindowAttributes attributes = {0};
            XGetWindowAttributes(display, window, &attributes);
            Window query_root = None;
            Window parent = None;
            Window *kids = NULL;
            unsigned count = 0;
            XQueryTree(display, window, &query_root, &parent, &kids, &count);
            if (kids != NULL) XFree(kids);
            printf("BXINFO find-%s 0x%lx parent=0x%lx %dx%d+%d+%d map=%d\n",
                   wanted, (unsigned long)window, (unsigned long)parent,
                   attributes.width, attributes.height, attributes.x,
                   attributes.y, attributes.map_state);
            accept_log_line("find-%s 0x%lx parent=0x%lx %dx%d+%d+%d map=%d\n",
                            wanted, (unsigned long)window,
                            (unsigned long)parent, attributes.width,
                            attributes.height, attributes.x, attributes.y,
                            attributes.map_state);
        }
        if (hint.res_name != NULL) XFree(hint.res_name);
        if (hint.res_class != NULL) XFree(hint.res_class);
    }
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &count)
            || kids == NULL)
        return;
    for (unsigned i = 0; i < count; ++i)
        dump_find_class(display, kids[i], root, wanted, depth + 1);
    XFree(kids);
}

static void dump_ancestors(Display *display, Window window, const char *tag) {
    Window current = window;
    for (int i = 0; i < 8 && current != None; ++i) {
        XWindowAttributes attributes = {0};
        Window query_root = None;
        Window parent = None;
        Window *kids = NULL;
        unsigned count = 0;
        if (!XGetWindowAttributes(display, current, &attributes)) {
            accept_log_line("%s 0x%lx gone\n", tag, (unsigned long)current);
            printf("BXINFO %s 0x%lx gone\n", tag, (unsigned long)current);
            break;
        }
        XQueryTree(display, current, &query_root, &parent, &kids, &count);
        if (kids != NULL) XFree(kids);
        printf("BXINFO %s #%d 0x%lx parent=0x%lx %dx%d+%d+%d map=%d or=%d\n",
               tag, i, (unsigned long)current, (unsigned long)parent,
               attributes.width, attributes.height, attributes.x, attributes.y,
               attributes.map_state, attributes.override_redirect);
        accept_log_line("%s #%d 0x%lx parent=0x%lx %dx%d+%d+%d map=%d or=%d\n",
                        tag, i, (unsigned long)current, (unsigned long)parent,
                        attributes.width, attributes.height, attributes.x,
                        attributes.y, attributes.map_state,
                        attributes.override_redirect);
        if (parent == None || parent == query_root) break;
        current = parent;
    }
    fflush(stdout);
}

static void dump_thin_root(Display *display, Window root) {
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, root, &query_root, &parent, &kids, &count)
            || kids == NULL)
        return;
    unsigned found = 0;
    for (unsigned i = 0; i < count; ++i) {
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, kids[i], &attributes)) continue;
        if (attributes.height > 80 || attributes.width < 200) continue;
        printf("BXINFO thin-root 0x%lx %dx%d+%d+%d map=%d or=%d\n",
               (unsigned long)kids[i], attributes.width, attributes.height,
               attributes.x, attributes.y, attributes.map_state,
               attributes.override_redirect);
        accept_log_line("thin-root 0x%lx %dx%d+%d+%d map=%d or=%d\n",
                        (unsigned long)kids[i], attributes.width,
                        attributes.height, attributes.x, attributes.y,
                        attributes.map_state, attributes.override_redirect);
        found++;
    }
    if (found == 0) {
        printf("BXINFO thin-root none count=%u\n", count);
        accept_log_line("thin-root none count=%u\n", count);
    }
    XFree(kids);
    fflush(stdout);
}

static void dump_root_stack(Display *display, Window root) {
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, root, &query_root, &parent, &kids, &count)
            || kids == NULL) {
        printf("BXINFO root-stack none\n");
        fflush(stdout);
        return;
    }
    unsigned printed = 0;
    for (unsigned n = 0; n < count && printed < 16; ++n) {
        Window window = kids[count - 1 - n];
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, window, &attributes)) continue;
        if (attributes.map_state == IsUnmapped) continue;
        printed++;
        XClassHint hint = {0};
        const char *cls = "none";
        if (XGetClassHint(display, window, &hint)) {
            cls = hint.res_class != NULL ? hint.res_class
                    : (hint.res_name != NULL ? hint.res_name : "none");
        }
        char type[64];
        dump_window_type(display, window, type, sizeof(type));
        Window child = None;
        Window child_root = None;
        Window child_parent = None;
        Window *frame_kids = NULL;
        unsigned frame_count = 0;
        if (XQueryTree(display, window, &child_root, &child_parent,
                       &frame_kids, &frame_count) && frame_kids != NULL) {
            for (unsigned k = 0; k < frame_count && child == None; ++k) {
                XClassHint probe = {0};
                if (XGetClassHint(display, frame_kids[k], &probe)) {
                    child = frame_kids[k];
                    if (probe.res_name != NULL) XFree(probe.res_name);
                    if (probe.res_class != NULL) XFree(probe.res_class);
                }
            }
            XFree(frame_kids);
        }
        char child_type[64];
        dump_window_type(display, child != None ? child : window, child_type,
                         sizeof(child_type));
        XClassHint child_hint = {0};
        const char *child_cls = "none";
        if (child != None && XGetClassHint(display, child, &child_hint)) {
            child_cls = child_hint.res_class != NULL ? child_hint.res_class
                    : (child_hint.res_name != NULL ? child_hint.res_name
                            : "none");
        }
        printf("BXINFO root-stack #%u 0x%lx %dx%d+%d+%d map=%d class=%s "
               "type=%s child=0x%lx child_class=%s child_type=%s\n",
               printed - 1, (unsigned long)window, attributes.width, attributes.height,
               attributes.x, attributes.y, attributes.map_state, cls, type,
               (unsigned long)child, child_cls, child_type);
        accept_log_line("root-stack #%u 0x%lx %dx%d+%d+%d class=%s type=%s "
                        "child_class=%s child_type=%s\n",
                        printed - 1, (unsigned long)window,
                        attributes.width, attributes.height, attributes.x,
                        attributes.y, cls, type, child_cls, child_type);
        if (hint.res_name != NULL) XFree(hint.res_name);
        if (hint.res_class != NULL) XFree(hint.res_class);
        if (child_hint.res_name != NULL) XFree(child_hint.res_name);
        if (child_hint.res_class != NULL) XFree(child_hint.res_class);
    }
    XFree(kids);
    fflush(stdout);
}

static void dump_session_click(Display *display, Window root, Window bar,
                               int click_x, int click_y) {
    KeyCode escape = XKeysymToKeycode(display, XK_Escape);
    if (escape != 0) {
        XTestFakeKeyEvent(display, escape, True, 0);
        XTestFakeKeyEvent(display, escape, False, 20);
        XSync(display, False);
        sleep_ms(300);
    }
    const char *home = getenv("HOME");
    if (home != NULL) {
        char path[512];
        snprintf(path, sizeof(path), "%s/bx-accept.log", home);
        accept_log = fopen(path, "w");
    }
    dump_override(display, root, "pre-click-or");
    dump_root_stack(display, root);
    dump_thin_root(display, root);
    dump_find_class(display, root, root, "Xfce4-panel", 0);
    dump_find_class(display, root, root, "xfce4-panel", 0);
    dump_find_class(display, root, root, "Wrapper-2.0", 0);
    if (bar != None) {
        XWindowAttributes bar_attr = {0};
        Window query_root = None;
        Window parent = None;
        Window *kids = NULL;
        unsigned count = 0;
        XGetWindowAttributes(display, bar, &bar_attr);
        XQueryTree(display, bar, &query_root, &parent, &kids, &count);
        if (kids != NULL) XFree(kids);
        printf("BXINFO saved-bar 0x%lx parent=0x%lx %dx%d+%d+%d map=%d\n",
               (unsigned long)bar, (unsigned long)parent, bar_attr.width,
               bar_attr.height, bar_attr.x, bar_attr.y, bar_attr.map_state);
        accept_log_line("saved-bar 0x%lx parent=0x%lx %dx%d+%d+%d map=%d\n",
                        (unsigned long)bar, (unsigned long)parent,
                        bar_attr.width, bar_attr.height, bar_attr.x,
                        bar_attr.y, bar_attr.map_state);
        dump_ancestors(display, bar, "bar-anc");
        if (parent != None && parent != root)
            dump_ancestors(display, parent, "frame-anc");
        dump_children(display, bar, "bar-child");
    }
    dump_class_paint(display, root, "Mousepad", "mp-paint");
    dump_or_paint(display, root, "pre-click-tip");
    int grab = grab_free(display, root);
    printf("BXINFO grab-state %d click=%d,%d bar=0x%lx\n", grab, click_x,
           click_y, (unsigned long)bar);
    fflush(stdout);
    if (bar == None || click_x < 0 || click_y < 0) return;
    Window existing[32];
    int existing_n = collect_override(display, root, existing, 32, 1);
    XTestFakeMotionEvent(display, DefaultScreen(display), click_x, click_y, 0);
    XSync(display, False);
    dump_pointer(display, root, "pre-click-ptr");
    int grab_motion = grab_free(display, root);
    printf("BXINFO grab-motion %d\n", grab_motion);
    fflush(stdout);
    long gdk_from = file_size_of(gdk_ev_log);
    XTestFakeButtonEvent(display, 1, True, 30);
    XTestFakeButtonEvent(display, 1, False, 30);
    XSync(display, False);
    char detail[128];
    int menu = wait_new_menu(display, root, existing, existing_n, 1500, detail,
                             sizeof(detail));
    dump_pointer(display, root, "post-click-ptr");
    dump_override(display, root, "post-click-or");
    dump_or_paint(display, root, "post-click-tip");
    dump_find_class(display, root, root, "Xfce4-panel", 0);
    int grab_after = grab_free(display, root);
    dump_gdk_ev(gdk_ev_log, gdk_from);
    printf("BXINFO click-menu %s grab=%d->%d %s\n",
           menu ? "opened" : "none", grab, grab_after,
           menu ? detail : "no popup");
    fflush(stdout);
    accept_log_line("click=%d,%d bar=0x%lx menu=%d grab=%d->%d %s\n",
                    click_x, click_y, (unsigned long)bar, menu, grab,
                    grab_after, menu ? detail : "no popup");
    if (accept_log != NULL) {
        fclose(accept_log);
        accept_log = NULL;
    }
}

static int accept_session(Display *display, const char *prefix,
                          char *app2_path) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[256];

    result("session-display", 1, XDisplayString(display));
    ++passed;
    Window root = DefaultRootWindow(display);

    int wm_ok = 0;
    for (int i = 0; i < 80 && !wm_ok; ++i) {
        wm_ok = wm_is_xfwm(display, root);
        if (!wm_ok) sleep_ms(100);
    }
    result("xfce-wm", wm_ok, wm_ok ? "xfwm4" : "no supporting WM");
    RECORD(wm_ok);

    int cm_ok = 0;
    for (int i = 0; i < 40 && !cm_ok; ++i) {
        cm_ok = compositor_selection_owned(display);
        if (!cm_ok) sleep_ms(100);
    }
    result("xfce-compositor", cm_ok,
           cm_ok ? "_NET_WM_CM_S0" : "compositor selection unowned");
    RECORD(cm_ok);

    int settings_ok = 0;
    for (int i = 0; i < 40 && !settings_ok; ++i) {
        settings_ok = named_selection_owned(display, "_XSETTINGS_S0");
        if (!settings_ok) sleep_ms(100);
    }
    result("xfce-settings", settings_ok,
           settings_ok ? "_XSETTINGS_S0" : "settings selection unowned");
    RECORD(settings_ok);

    Window panel = wait_class(display, root, "Xfce4-panel", 8000);
    if (panel == None)
        panel = wait_class(display, root, "xfce4-panel", 1000);
    if (panel == None)
        panel = wait_class(display, root, "Wrapper-2.0", 1000);
    result("xfce-panel", panel != None,
           panel != None ? "mapped" : "panel not mapped");
    RECORD(panel != None);
    if (panel == None) dump_mapped(display, root);
    Window bar = find_widest_class(display, root, "Xfce4-panel");
    if (bar == None) bar = find_widest_class(display, root, "xfce4-panel");
    if (bar == None) bar = panel;
    int saved_click_x = -1;
    int saved_click_y = -1;
    Window saved_bar = bar;
    if (bar != None) {
        XWindowAttributes bar_attr = {0};
        if (XGetWindowAttributes(display, bar, &bar_attr)) {
            int local_x = bar_attr.width > 24 ? 12 : bar_attr.width / 2;
            int local_y = bar_attr.height > 2 ? bar_attr.height / 2 : 12;
            int root_x = 0;
            int root_y = 0;
            Window ignore = None;
            XTranslateCoordinates(display, bar, root, local_x, local_y,
                                  &root_x, &root_y, &ignore);
            saved_click_x = root_x;
            saved_click_y = root_y;
            XTestFakeMotionEvent(display, DefaultScreen(display), root_x,
                                 root_y, 0);
            XSync(display, False);
            Window qroot = None;
            Window qchild = None;
            int qx = 0, qy = 0, wx = 0, wy = 0;
            unsigned qmask = 0;
            XQueryPointer(display, root, &qroot, &qchild, &qx, &qy, &wx, &wy,
                          &qmask);
            Window bar_child = None;
            XQueryPointer(display, bar, &qroot, &bar_child, &qx, &qy, &wx, &wy,
                          &qmask);
            Window parent = None;
            Window query_root = None;
            Window *kids = NULL;
            unsigned count = 0;
            XQueryTree(display, bar, &query_root, &parent, &kids, &count);
            if (kids != NULL) XFree(kids);
            XClassHint hint = {0};
            const char *cls = "none";
            if (qchild != None && XGetClassHint(display, qchild, &hint)) {
                cls = hint.res_class != NULL ? hint.res_class
                        : (hint.res_name != NULL ? hint.res_name : "none");
            }
            XWindowAttributes frame_attr = {0};
            Window frame_child = None;
            if (parent != None) {
                XGetWindowAttributes(display, parent, &frame_attr);
                XQueryPointer(display, parent, &qroot, &frame_child, &qx, &qy,
                              &wx, &wy, &qmask);
            }
            printf("BXINFO click-hit bar=0x%lx %dx%d+%d+%d click=%d,%d "
                   "root_child=0x%lx class=%s bar_child=0x%lx parent=0x%lx "
                   "frame=%dx%d+%d+%d frame_child=0x%lx\n",
                   (unsigned long)bar, bar_attr.width, bar_attr.height,
                   bar_attr.x, bar_attr.y, root_x, root_y,
                   (unsigned long)qchild, cls, (unsigned long)bar_child,
                   (unsigned long)parent, frame_attr.width, frame_attr.height,
                   frame_attr.x, frame_attr.y, (unsigned long)frame_child);
            if (hint.res_name != NULL) XFree(hint.res_name);
            if (hint.res_class != NULL) XFree(hint.res_class);
            fflush(stdout);
        }
    }

    Window thunar = wait_class(display, root, "Thunar", 12000);
    Window mousepad = wait_class(display, root, "Mousepad", 8000);
    int mapped = thunar != None && mousepad != None;
    snprintf(detail, sizeof(detail), "thunar=0x%lx mousepad=0x%lx",
             (unsigned long)thunar, (unsigned long)mousepad);
    result("session-two-mapped", mapped, detail);
    RECORD(mapped);

    int menu_ok = 0;
    KeyCode escape = XKeysymToKeycode(display, XK_Escape);
    if (escape != 0) {
        XTestFakeKeyEvent(display, escape, True, 0);
        XTestFakeKeyEvent(display, escape, False, 20);
        XSync(display, False);
        sleep_ms(200);
    }
    Window existing[32];
    int existing_n = collect_override(display, root, existing, 32, 0);
    char popup[512];
    snprintf(popup, sizeof(popup), "%s/xfce4-popup-applicationsmenu", prefix);
    char *popup_argv[] = {popup, NULL};
    pid_t popup_pid = start(popup_argv);
    printf("BXINFO applications-popup pid=%d before_or=%d\n",
           (int)popup_pid, existing_n);
    fflush(stdout);
    menu_ok = wait_new_menu(display, root, existing, existing_n, 2000, detail,
                            sizeof(detail));
    if (!menu_ok) {
        Window bar = find_widest_class(display, root, "Xfce4-panel");
        if (bar == None) bar = find_widest_class(display, root, "xfce4-panel");
        if (bar == None) bar = panel;
        XWindowAttributes bar_attr = {0};
        int event_base = 0;
        int error_base = 0;
        int major = 0;
        int minor = 0;
        if (bar != None
                && XTestQueryExtension(display, &event_base, &error_base,
                                       &major, &minor)
                && XGetWindowAttributes(display, bar, &bar_attr)) {
            int root_x = 0;
            int root_y = 0;
            Window child = None;
            int local_x = bar_attr.width > 24 ? 12 : bar_attr.width / 2;
            int local_y = bar_attr.height > 2 ? bar_attr.height / 2 : 12;
            XTranslateCoordinates(display, bar, root, local_x, local_y,
                                  &root_x, &root_y, &child);
            printf("BXINFO applications-click bar=0x%lx click=%d,%d\n",
                   (unsigned long)bar, root_x, root_y);
            fflush(stdout);
            XTestFakeMotionEvent(display, DefaultScreen(display), root_x,
                                 root_y, 0);
            XTestFakeButtonEvent(display, 1, True, 30);
            XTestFakeButtonEvent(display, 1, False, 30);
            XSync(display, False);
            char click_detail[256];
            menu_ok = wait_new_menu(display, root, existing, existing_n, 2000,
                                    click_detail, sizeof(click_detail));
            if (menu_ok) {
                snprintf(detail, sizeof(detail), "%s", click_detail);
            } else {
                snprintf(detail, sizeof(detail),
                         "no popup after helper and click %d,%d",
                         root_x, root_y);
            }
        } else {
            snprintf(detail, sizeof(detail),
                     "xfce4-popup-applicationsmenu did not map a menu");
        }
    }
    result("session-applications-menu", menu_ok, detail);
    RECORD(menu_ok);

    if (!mapped) {
        printf("BXSUMMARY xfce-session-accept passed=%d failed=%d\n",
               passed, failed);
        return 1;
    }

    activate(display, thunar);
    int thunar_focus = wait_focus(display, thunar, 2500);
    result("session-switch-thunar", thunar_focus,
           thunar_focus ? "focus=thunar" : "focus not thunar");
    RECORD(thunar_focus);

    activate(display, mousepad);
    int mousepad_focus = wait_focus(display, mousepad, 2500);
    result("session-switch-mousepad", mousepad_focus,
           mousepad_focus ? "focus=mousepad" : "focus not mousepad");
    RECORD(mousepad_focus);

    XWindowAttributes before = {0};
    XGetWindowAttributes(display, thunar, &before);
    int screen = DefaultScreen(display);
    unsigned new_width = (unsigned)before.width + 96;
    unsigned new_height = (unsigned)before.height + 48;
    if (before.width + 96 >= DisplayWidth(display, screen)
            || before.height + 48 >= DisplayHeight(display, screen)) {
        if (before.width > 200)
            new_width = (unsigned)before.width - 96;
        if (before.height > 120)
            new_height = (unsigned)before.height - 48;
    }
    XResizeWindow(display, thunar, new_width, new_height);
    XSync(display, False);
    int resized = 0;
    for (int i = 0; i < 20; ++i) {
        XWindowAttributes after = {0};
        XGetWindowAttributes(display, thunar, &after);
        if (after.width != before.width || after.height != before.height) {
            snprintf(detail, sizeof(detail), "%dx%d -> %dx%d",
                     before.width, before.height, after.width, after.height);
            resized = 1;
            break;
        }
        sleep_ms(100);
    }
    if (!resized) {
        snprintf(detail, sizeof(detail), "geometry unchanged %dx%d",
                 before.width, before.height);
    }
    result("session-resize-thunar", resized, detail);
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

    dump_typed_text(display, root);
    dump_session_click(display, root, saved_bar, saved_click_x, saved_click_y);

    printf("BXSUMMARY xfce-session-accept passed=%d failed=%d\n",
           passed, failed);
    fflush(stdout);
    return failed == 0 ? 0 : 1;
}

static void join_path(char *out, size_t size, const char *prefix,
                      const char *name) {
    snprintf(out, size, "%s/%s", prefix, name);
}

int main(int argc, char **argv) {
    int accept = 0;
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--accept") == 0) {
        accept = 1;
        ++argi;
    }
    if (argc - argi < 3) {
        fprintf(stderr, "usage: %s [--accept] PREFIX APP1 APP2\n", argv[0]);
        return 2;
    }
    signal(SIGTERM, stop_children);
    signal(SIGINT, stop_children);
    XSetErrorHandler(on_x_error);

    const char *prefix = argv[argi];
    char xfwm4[512];
    char settings[512];
    char panel[512];
    char desktop[512];
    join_path(xfwm4, sizeof(xfwm4), prefix, "xfwm4");
    join_path(settings, sizeof(settings), prefix, "xfsettingsd");
    join_path(panel, sizeof(panel), prefix, "xfce4-panel");
    join_path(desktop, sizeof(desktop), prefix, "xfdesktop");

    char *wm_argv[] = {xfwm4, "--compositor=on", "--vblank=off",
                       "--sm-client-disable", NULL};
    char *settings_argv[] = {settings, "--disable-wm-check",
                             "--sm-client-disable", NULL};
    char *panel_argv[] = {panel, "--disable-wm-check", "--sm-client-disable",
                          NULL};
    char *desktop_argv[] = {desktop, "--disable-wm-check", NULL};
    char *app1[] = {argv[argi + 1], NULL};
    char *app2[] = {argv[argi + 2], NULL};

    Display *display = XOpenDisplay(NULL);
    if (display != NULL) {
        int fd = ConnectionNumber(display);
        if (fd >= 0) fcntl(fd, F_SETFD, FD_CLOEXEC);
    }

    const char *home = getenv("HOME");
    snprintf(gdk_ev_log, sizeof(gdk_ev_log), "%s/bx-gdk-events.log",
             home != NULL ? home : "/tmp");
    start_logged(wm_argv, gdk_ev_log, "events");
    if (display != NULL) {
        Window root = DefaultRootWindow(display);
        for (int i = 0; i < 80 && !wm_is_xfwm(display, root); ++i)
            sleep_ms(100);
        sleep_ms(1200);
    } else {
        sleep_ms(2000);
    }

    start(settings_argv);
    sleep_ms(400);
    start(panel_argv);
    sleep_ms(400);
    /* xfdesktop D-Bus-activates org.xfce.FileManager. Start Thunar first
     * so it owns org.xfce.Thunar instead of racing a second instance. */
    start(app1);
    sleep_ms(400);
    start(desktop_argv);
    sleep_ms(400);
    start(app2);
    printf("BXTEST PASS xfce-session-launch children=%d\n",
           (int)child_count);
    fflush(stdout);

    int accept_status = 0;
    if (accept) {
        sleep_ms(1500);
        if (display == NULL) {
            result("session-display", 0, "DISPLAY");
            accept_status = 1;
        } else {
            accept_status = accept_session(display, prefix, argv[argi + 2]);
        }
    }
    if (display != NULL) XCloseDisplay(display);

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
    printf("BXSUMMARY xfce-session waited=%d accept=%d\n",
           (int)child_count - remaining, accept_status);
    return accept ? accept_status : 0;
}
