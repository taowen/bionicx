#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void click_xy(Display *d, int screen, int x, int y) {
    /*
     * XTEST delay is milliseconds after the previous queued event.
     * CurrentTime/0 on motion+press+release delivers the click at the
     * previous pointer, so Qt never sees the intended widget.
     */
    XTestFakeMotionEvent(d, screen, x, y, 0);
    XTestFakeButtonEvent(d, 1, True, 40);
    XTestFakeButtonEvent(d, 1, False, 40);
}

static char *window_title(Display *d, Window window) {
    Atom net_wm_name = XInternAtom(d, "_NET_WM_NAME", False);
    Atom actual = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char *value = NULL;
    if (XGetWindowProperty(d, window, net_wm_name, 0, 256, False,
                           AnyPropertyType, &actual, &format, &count,
                           &remaining, &value) == Success &&
            value != NULL && format == 8 && count > 0) {
        char *title = calloc(count + 1, 1);
        if (title != NULL)
            memcpy(title, value, count);
        XFree(value);
        return title;
    }
    if (value != NULL)
        XFree(value);
    char *legacy = NULL;
    if (XFetchName(d, window, &legacy) && legacy != NULL) {
        char *title = strdup(legacy);
        XFree(legacy);
        return title;
    }
    return NULL;
}

static int click_named_window(Display *d, const char *needle, double fx,
                              double fy) {
    int screen = DefaultScreen(d);
    Window root = RootWindow(d, screen);
    Window root_ret = None;
    Window parent = None;
    Window *children = NULL;
    unsigned int child_count = 0;
    if (!XQueryTree(d, root, &root_ret, &parent, &children, &child_count))
        return 1;
    int rc = 1;
    for (unsigned int i = 0; i < child_count; ++i) {
        XWindowAttributes attributes;
        if (!XGetWindowAttributes(d, children[i], &attributes) ||
                attributes.map_state != IsViewable)
            continue;
        char *title = window_title(d, children[i]);
        if (title == NULL || strstr(title, needle) == NULL) {
            free(title);
            continue;
        }
        int x = attributes.x + (int)(fx * (double)attributes.width);
        int y = attributes.y + (int)(fy * (double)attributes.height);
        printf("BXINFO click-window title=%s geometry=%dx%d+%d+%d xy=%d,%d\n",
               title, attributes.width, attributes.height, attributes.x,
               attributes.y, x, y);
        click_xy(d, screen, x, y);
        free(title);
        rc = 0;
        break;
    }
    if (children != NULL)
        XFree(children);
    return rc;
}

static void tap(Display *d, KeyCode code) {
    XTestFakeKeyEvent(d, code, True, CurrentTime);
    XTestFakeKeyEvent(d, code, False, CurrentTime);
}

static void send_combo(Display *d, Window w, KeySym sym, unsigned modifiers) {
    (void)w;
    KeyCode code = XKeysymToKeycode(d, sym);
    KeyCode shift = XKeysymToKeycode(d, XK_Shift_L);
    KeyCode control = XKeysymToKeycode(d, XK_Control_L);
    if (modifiers & ControlMask)
        XTestFakeKeyEvent(d, control, True, CurrentTime);
    if (modifiers & ShiftMask)
        XTestFakeKeyEvent(d, shift, True, CurrentTime);
    tap(d, code);
    if (modifiers & ShiftMask)
        XTestFakeKeyEvent(d, shift, False, CurrentTime);
    if (modifiers & ControlMask)
        XTestFakeKeyEvent(d, control, False, CurrentTime);
}

static int type_ascii(Display *d, Window w, const char *text) {
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        KeySym sym = NoSymbol;
        unsigned modifiers = 0;
        unsigned char ch = *p;
        if (ch >= 'a' && ch <= 'z') {
            sym = XK_a + (ch - 'a');
        } else if (ch >= 'A' && ch <= 'Z') {
            sym = XK_A + (ch - 'A');
            modifiers = ShiftMask;
        } else if (ch >= '0' && ch <= '9') {
            sym = XK_0 + (ch - '0');
        } else if (ch == '_') {
            sym = XK_underscore;
            modifiers = ShiftMask;
        } else if (ch == ' ') {
            sym = XK_space;
        } else if (ch == '=') {
            sym = XK_equal;
        } else if (ch == '+') {
            sym = XK_plus;
            modifiers = ShiftMask;
        } else if (ch == ':') {
            sym = XK_colon;
            modifiers = ShiftMask;
        } else if (ch == '-') {
            sym = XK_minus;
        } else if (ch == '.') {
            sym = XK_period;
        } else if (ch == '/') {
            sym = XK_slash;
        } else if (ch == '(') {
            sym = XK_parenleft;
            modifiers = ShiftMask;
        } else if (ch == ')') {
            sym = XK_parenright;
            modifiers = ShiftMask;
        } else {
            fprintf(stderr, "unsupported char 0x%02x\n", ch);
            return 2;
        }
        send_combo(d, w, sym, modifiers);
        usleep(8000);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: x11-send-key escape|return|tab|space|f5|"
                "ctrl-a|ctrl-c|ctrl-v|ctrl-s|ctrl-p|end|type TEXT|"
                "click X Y|click-frac FX FY|click-window TITLE FX FY\n");
        return 2;
    }
    Display *d = XOpenDisplay(NULL);
    if (d == NULL) {
        fprintf(stderr, "no display\n");
        return 1;
    }
    Window focus = None;
    int revert = 0;
    XGetInputFocus(d, &focus, &revert);
    if (focus == None || focus == PointerRoot)
        focus = DefaultRootWindow(d);
    printf("BXTEST PASS x11-send-key focus=0x%lx cmd=%s\n",
           (unsigned long)focus, argv[1]);
    int rc = 0;
    if (strcmp(argv[1], "escape") == 0)
        send_combo(d, focus, XK_Escape, 0);
    else if (strcmp(argv[1], "return") == 0)
        send_combo(d, focus, XK_Return, 0);
    else if (strcmp(argv[1], "tab") == 0)
        send_combo(d, focus, XK_Tab, 0);
    else if (strcmp(argv[1], "space") == 0)
        send_combo(d, focus, XK_space, 0);
    else if (strcmp(argv[1], "f5") == 0)
        send_combo(d, focus, XK_F5, 0);
    else if (strcmp(argv[1], "end") == 0)
        send_combo(d, focus, XK_End, 0);
    else if (strcmp(argv[1], "ctrl-a") == 0)
        send_combo(d, focus, XK_a, ControlMask);
    else if (strcmp(argv[1], "ctrl-c") == 0)
        send_combo(d, focus, XK_c, ControlMask);
    else if (strcmp(argv[1], "ctrl-v") == 0)
        send_combo(d, focus, XK_v, ControlMask);
    else if (strcmp(argv[1], "ctrl-s") == 0)
        send_combo(d, focus, XK_s, ControlMask);
    else if (strcmp(argv[1], "ctrl-p") == 0)
        send_combo(d, focus, XK_p, ControlMask);
    else if (strcmp(argv[1], "type") == 0 && argc >= 3)
        rc = type_ascii(d, focus, argv[2]);
    else if (strcmp(argv[1], "click") == 0 && argc >= 4) {
        click_xy(d, DefaultScreen(d), atoi(argv[2]), atoi(argv[3]));
    } else if (strcmp(argv[1], "click-frac") == 0 && argc >= 4) {
        int screen = DefaultScreen(d);
        int width = DisplayWidth(d, screen);
        int height = DisplayHeight(d, screen);
        int x = (int)(atof(argv[2]) * (double)width);
        int y = (int)(atof(argv[3]) * (double)height);
        printf("BXINFO click-frac display=%dx%d xy=%d,%d\n",
               width, height, x, y);
        click_xy(d, screen, x, y);
    } else if (strcmp(argv[1], "click-window") == 0 && argc >= 5) {
        rc = click_named_window(d, argv[2], atof(argv[3]), atof(argv[4]));
        if (rc != 0)
            fprintf(stderr, "no viewable window title contains %s\n", argv[2]);
    } else {
        fprintf(stderr, "unknown command\n");
        rc = 2;
    }
    XFlush(d);
    XCloseDisplay(d);
    return rc;
}
