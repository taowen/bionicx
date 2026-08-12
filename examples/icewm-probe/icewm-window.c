#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void sleep_milliseconds(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = milliseconds % 1000 * 1000 * 1000,
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static unsigned int decoration_windows;
static unsigned int painted_decorations;

static void paint(Display* display, Window window, GC gc, int width,
                  int height, unsigned long color, const char* title) {
    XSetForeground(display, gc, color);
    XFillRectangle(display, window, gc, 0, 0, (unsigned int)width,
                   (unsigned int)height);
    XSetForeground(display, gc, 0xffffff);
    XDrawString(display, window, gc, 32, 54, title, (int)strlen(title));
    const char* detail = "real glibc X11 client managed by IceWM";
    XDrawString(display, window, gc, 32, 86, detail, (int)strlen(detail));
    XFlush(display);
}

static int find_mapped_dock(Display* display, Window window, Atom type_atom,
                            Atom dock_atom, int depth) {
    if (depth > 8) return 0;
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char* value = NULL;
    XWindowAttributes attributes;
    int have_attributes = XGetWindowAttributes(display, window, &attributes);
    if (have_attributes && attributes.map_state == IsViewable && depth > 0) {
        char* name = NULL;
        XFetchName(display, window, &name);
        int named_taskbar = name != NULL && strcmp(name, "TaskBar") == 0 &&
                attributes.width >= DisplayWidth(display, DefaultScreen(display)) / 2 &&
                attributes.height > 0 && attributes.height <= 128;
        if (named_taskbar)
            printf("BXICEWM taskbar window=0x%lx geometry=%dx%d+%d+%d\n",
                   window, attributes.width, attributes.height,
                   attributes.x, attributes.y);
        if (name != NULL) XFree(name);
        if (named_taskbar) return 1;
    }
    if (have_attributes && attributes.map_state == IsViewable &&
            XGetWindowProperty(display, window, type_atom, 0, 16, False,
                XA_ATOM, &actual_type, &actual_format, &item_count,
                &bytes_after, &value) == Success) {
        Atom* atoms = (Atom*)value;
        for (unsigned long index = 0; index < item_count; ++index) {
            if (atoms[index] == dock_atom) {
                printf("BXICEWM taskbar window=0x%lx geometry=%dx%d+%d+%d\n",
                       window, attributes.width, attributes.height,
                       attributes.x, attributes.y);
                XFree(value);
                return 1;
            }
        }
    }
    if (value != NULL) XFree(value);

    Window root = None;
    Window parent = None;
    Window* children = NULL;
    unsigned int child_count = 0;
    if (!XQueryTree(display, window, &root, &parent, &children, &child_count))
        return 0;
    int found = 0;
    for (unsigned int index = 0; index < child_count && !found; ++index)
        found = find_mapped_dock(display, children[index], type_atom,
                                 dock_atom, depth + 1);
    if (children != NULL) XFree(children);
    return found;
}

static int check_taskbar(Display* display) {
    Atom type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
    Atom dock_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", True);
    if (type_atom == None || dock_atom == None) return 0;
    XSync(display, False);
    return find_mapped_dock(display, DefaultRootWindow(display), type_atom,
                            dock_atom, 0);
}

static unsigned int dump_tree(Display* display, Window window, int depth) {
    Window root = None;
    Window parent = None;
    Window* children = NULL;
    unsigned int child_count = 0;
    if (depth > 12 || !XQueryTree(display, window, &root, &parent,
                                  &children, &child_count)) return 0;

    unsigned int count = 0;
    for (unsigned int index = 0; index < child_count; ++index) {
        XWindowAttributes attributes;
        if (!XGetWindowAttributes(display, children[index], &attributes))
            continue;
        char* name = NULL;
        XFetchName(display, children[index], &name);
        printf("BXICEWM tree depth=%d id=0x%lx parent=0x%lx map=%d class=%d "
               "events=0x%lx geometry=%dx%d%+d%+d title=%s\n",
               depth, children[index], window, attributes.map_state,
               attributes.class, attributes.all_event_masks,
               attributes.width, attributes.height,
               attributes.x, attributes.y, name != NULL ? name : "-");
        if (attributes.class == InputOutput && attributes.map_state == IsViewable
                && name != NULL && (strcmp(name, "TitleBar") == 0
                || strcmp(name, "Close") == 0
                || strcmp(name, "TaskBar") == 0)) {
            XImage* image = XGetImage(display, children[index], 0, 0,
                    (unsigned int)attributes.width,
                    (unsigned int)attributes.height, AllPlanes, ZPixmap);
            if (image != NULL) {
                unsigned long nonzero = 0;
                unsigned long different = 0;
                unsigned long first = XGetPixel(image, 0, 0);
                unsigned long hash = 1469598103934665603UL;
                for (int y = 0; y < attributes.height; ++y) {
                    for (int x = 0; x < attributes.width; ++x) {
                        unsigned long pixel = XGetPixel(image, x, y);
                        if (pixel != 0) nonzero++;
                        if (pixel != first) different++;
                        hash ^= pixel;
                        hash *= 1099511628211UL;
                    }
                }
                printf("BXICEWM pixels title=%s nonzero=%lu different=%lu "
                       "first=0x%lx hash=0x%lx\n", name, nonzero,
                       different, first, hash);
                if (strcmp(name, "TitleBar") == 0
                        || strcmp(name, "Close") == 0) {
                    decoration_windows++;
                    if (nonzero == (unsigned long)attributes.width
                            * (unsigned long)attributes.height
                            && different > 0) painted_decorations++;
                }
                XDestroyImage(image);
            }
        }
        if (name != NULL) XFree(name);
        count++;
        count += dump_tree(display, children[index], depth + 1);
    }
    if (children != NULL) XFree(children);
    return count;
}

int main(int argc, char** argv) {
    const char* title = argc > 1 ? argv[1] : "BionicX desktop window";
    int x = argc > 2 ? atoi(argv[2]) : 100;
    int y = argc > 3 ? atoi(argv[3]) : 100;
    unsigned long color = argc > 4 ? strtoul(argv[4], NULL, 0) : 0x285078;
    int duration = argc > 5 ? atoi(argv[5]) : 20;

    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXICEWM FAIL client-display title=%s\n", title);
        return 2;
    }
    if (argc > 1 && strcmp(argv[1], "--check-taskbar") == 0) {
        int found = check_taskbar(display);
        printf("BXTEST %s icewm-taskbar-mapped\n", found ? "PASS" : "FAIL");
        XCloseDisplay(display);
        return found ? 0 : 4;
    }
    if (argc > 1 && strcmp(argv[1], "--dump-tree") == 0) {
        unsigned int count = dump_tree(display, DefaultRootWindow(display), 0);
        XSync(display, False);
        printf("BXICEWM tree-summary windows=%u\n", count);
        int decorations_ok = decoration_windows >= 4
                && painted_decorations == decoration_windows;
        printf("BXTEST %s icewm-decoration-pixels painted=%u/%u\n",
               decorations_ok ? "PASS" : "FAIL", painted_decorations,
               decoration_windows);
        XCloseDisplay(display);
        return count > 0 && decorations_ok ? 0 : 6;
    }
    int screen = DefaultScreen(display);
    int width = 620;
    int height = 310;
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
            x, y, (unsigned int)width, (unsigned int)height, 0,
            BlackPixel(display, screen), color);
    XStoreName(display, window, title);
    XSelectInput(display, window, ExposureMask | StructureNotifyMask |
            FocusChangeMask);
    GC gc = XCreateGC(display, window, 0, NULL);
    XMapWindow(display, window);
    XFlush(display);

    int mapped = 0;
    int configured = 0;
    int focused = 0;
    for (int tick = 0; tick < duration * 20; ++tick) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == MapNotify) mapped = 1;
            if (event.type == FocusIn) focused++;
            if (event.type == ConfigureNotify) {
                width = event.xconfigure.width;
                height = event.xconfigure.height;
                configured++;
            }
            if (event.type == Expose || event.type == ConfigureNotify)
                paint(display, window, gc, width, height, color, title);
        }
        sleep_milliseconds(50);
    }

    printf("BXICEWM client title=%s mapped=%d configured=%d focused=%d size=%dx%d\n",
           title, mapped, configured, focused, width, height);
    fflush(stdout);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return mapped && configured > 0 ? 0 : 3;
}
