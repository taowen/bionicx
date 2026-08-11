#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <errno.h>
#include <gnu/libc-version.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static int x_errors;
static char last_x_error[256];

static int on_x_error(Display *display, XErrorEvent *event) {
    char message[128];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    snprintf(last_x_error, sizeof(last_x_error),
             "%s request=%u minor=%u resource=0x%lx", message,
             event->request_code, event->minor_code, event->resourceid);
    ++x_errors;
    return 0;
}

static void result(const char *name, bool passed, const char *detail) {
    printf("BXTEST %s %s%s%s\n", passed ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail && *detail ? detail : "");
    fflush(stdout);
}

static bool sync_step(Display *display, const char *name, int before) {
    XSync(display, False);
    bool passed = x_errors == before;
    result(name, passed, passed ? "" : last_x_error);
    return passed;
}

static double monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static void paint(Display *display, Window window, GC gc, int width, int height,
                  int passed, int failed, int keys, int buttons, int motions) {
    unsigned long colors[] = {0x18233a, 0x157f3b, 0xc17b16, 0xb83a3a};
    XSetForeground(display, gc, colors[0]);
    XFillRectangle(display, window, gc, 0, 0, (unsigned)width, (unsigned)height);
    XSetForeground(display, gc, colors[1]);
    XFillRectangle(display, window, gc, 32, 100,
                   (unsigned)(width / 2 - 48), (unsigned)(height - 150));
    XSetForeground(display, gc, failed ? colors[3] : colors[2]);
    XFillRectangle(display, window, gc, width / 2 + 16, 100,
                   (unsigned)(width / 2 - 48), (unsigned)(height - 150));
    (void)passed;
    (void)keys;
    (void)buttons;
    (void)motions;
    XFlush(display);
}

int main(int argc, char **argv) {
    int duration = 12;
    if (argc == 3 && strcmp(argv[1], "--duration") == 0) {
        duration = atoi(argv[2]);
        if (duration < 1 || duration > 300) {
            fprintf(stderr, "duration must be in 1..300 seconds\n");
            return 2;
        }
    }

    XSetErrorHandler(on_x_error);
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        result("display-connect", false, getenv("DISPLAY"));
        return 1;
    }

    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[512];
    snprintf(detail, sizeof(detail), "glibc=%s vendor=%s protocol=%d.%d screens=%d",
             gnu_get_libc_version(), ServerVendor(display),
             ProtocolVersion(display), ProtocolRevision(display),
             ScreenCount(display));
    result("display-connect", true, detail);
    ++passed;

    int extension_count = 0;
    char **extensions = XListExtensions(display, &extension_count);
    snprintf(detail, sizeof(detail), "count=%d", extension_count);
    bool extensions_ok = extensions != NULL && extension_count > 0;
    result("list-extensions", extensions_ok,
           extensions_ok ? detail : "server returned no advertised extensions");
    RECORD(extensions_ok);
    for (int index = 0; index < extension_count; ++index)
        printf("BXINFO extension %s\n", extensions[index]);
    if (extensions) XFreeExtensionList(extensions);

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    Window window = XCreateSimpleWindow(display, root, 0, 0,
                                         (unsigned)width, (unsigned)height, 0,
                                         BlackPixel(display, screen),
                                         BlackPixel(display, screen));
    XSelectInput(display, window, ExposureMask | StructureNotifyMask |
                 KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | FocusChangeMask | PropertyChangeMask);
    XStoreName(display, window, "BionicX X11 integration probe");
    XClassHint class_hint = {.res_name = "bionicx-probe",
                             .res_class = "BionicXProbe"};
    XSetClassHint(display, window, &class_hint);
    XWMHints wm_hints = {.flags = WindowGroupHint, .window_group = window};
    XSetWMHints(display, window, &wm_hints);
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    Atom probe_property = XInternAtom(display, "BIONICX_TEST_PROPERTY", False);
    XSetWMProtocols(display, window, &wm_delete, 1);
    int before = x_errors;
    XMapWindow(display, window);
    RECORD(sync_step(display, "window-create-map", before));

    before = x_errors;
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    RECORD(sync_step(display, "input-focus", before));

    const char payload[] = "bionicx-property-roundtrip";
    before = x_errors;
    XChangeProperty(display, window, probe_property, utf8, 8, PropModeReplace,
                    (const unsigned char *)payload, (int)strlen(payload));
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long remaining = 0;
    unsigned char *property = NULL;
    int property_status = XGetWindowProperty(
            display, window, probe_property, 0, 1024, False, utf8,
            &actual_type, &actual_format, &items, &remaining, &property);
    XSync(display, False);
    bool property_ok = x_errors == before && property_status == Success &&
            actual_type == utf8 && actual_format == 8 &&
            items == strlen(payload) && property &&
            memcmp(property, payload, items) == 0;
    result("property-roundtrip", property_ok,
           property_ok ? "UTF8_STRING" : last_x_error);
    RECORD(property_ok);
    if (property) XFree(property);

    before = x_errors;
    Pixmap pixmap = XCreatePixmap(display, window, 160, 100,
                                  (unsigned)DefaultDepth(display, screen));
    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, 0x3264c8);
    XFillRectangle(display, pixmap, gc, 0, 0, 160, 100);
    XCopyArea(display, pixmap, window, gc, 0, 0, 160, 100, 40, 160);
    XImage *pixmap_image = XGetImage(display, pixmap, 0, 0, 1, 1,
                                     AllPlanes, ZPixmap);
    XImage *window_image = XGetImage(display, window, 40, 160, 1, 1,
                                     AllPlanes, ZPixmap);
    XSync(display, False);
    bool drawing_ok = x_errors == before && pixmap_image && window_image &&
                      (XGetPixel(pixmap_image, 0, 0) & 0x00ffffff) == 0x3264c8 &&
                      (XGetPixel(window_image, 0, 0) & 0x00ffffff) == 0x3264c8;
    if (pixmap_image) XDestroyImage(pixmap_image);
    if (window_image) XDestroyImage(window_image);
    result("pixmap-gc-copy", drawing_ok,
           drawing_ok ? "pixel=0x3264c8" : "pixel readback mismatch");
    RECORD(drawing_ok);

    before = x_errors;
    XSetForeground(display, gc, WhitePixel(display, screen));
    XDrawString(display, window, gc, 32, 42, "BionicX PolyText8", 17);
    XImage *text_image = XGetImage(display, window, 28, 20, 220, 30,
                                   AllPlanes, ZPixmap);
    XSync(display, False);
    int text_pixels = 0;
    if (text_image) {
        for (int text_y = 0; text_y < text_image->height; ++text_y)
            for (int text_x = 0; text_x < text_image->width; ++text_x)
                if ((XGetPixel(text_image, text_x, text_y) & 0x00ffffff) != 0)
                    ++text_pixels;
        XDestroyImage(text_image);
    }
    bool text_ok = x_errors == before && text_pixels > 0;
    snprintf(detail, sizeof(detail), "foreground-pixels=%d", text_pixels);
    result("poly-text8", text_ok, text_ok ? detail : last_x_error);
    RECORD(text_ok);

    before = x_errors;
    Window child = XCreateSimpleWindow(display, window, 220, 160, 160, 100, 1,
                                        WhitePixel(display, screen), 0x6c3ba8);
    XMapWindow(display, child);
    Window query_root = None;
    Window query_parent = None;
    Window *children = NULL;
    unsigned child_count = 0;
    Status tree_status = XQueryTree(display, window, &query_root, &query_parent,
                                    &children, &child_count);
    XSync(display, False);
    bool tree_ok = x_errors == before && tree_status && query_root == root &&
                   query_parent == root && child_count >= 1;
    snprintf(detail, sizeof(detail), "children=%u", child_count);
    result("window-tree", tree_ok, tree_ok ? detail : last_x_error);
    RECORD(tree_ok);
    if (children) XFree(children);

    int translated_x = 0;
    int translated_y = 0;
    Window translated_child = None;
    before = x_errors;
    Bool translated = XTranslateCoordinates(display, child, root, 0, 0,
                                             &translated_x, &translated_y,
                                             &translated_child);
    XSync(display, False);
    bool translate_ok = x_errors == before && translated;
    snprintf(detail, sizeof(detail), "root=%d,%d", translated_x, translated_y);
    result("translate-coordinates", translate_ok,
           translate_ok ? detail : last_x_error);
    RECORD(translate_ok);

    before = x_errors;
    static const char cursor_bits[] = {0x03, 0x03};
    Pixmap cursor_source = XCreateBitmapFromData(display, window, cursor_bits, 2, 2);
    XColor foreground = {.red = 0xffff, .green = 0xffff, .blue = 0xffff};
    XColor background = {0};
    Cursor cursor = XCreatePixmapCursor(display, cursor_source, cursor_source,
                                        &foreground, &background, 0, 0);
    XDefineCursor(display, window, cursor);
    RECORD(sync_step(display, "cursor-create-define", before));

    before = x_errors;
    XSetSelectionOwner(display, clipboard, window, CurrentTime);
    XSync(display, False);
    bool selection_ok = x_errors == before &&
                        XGetSelectionOwner(display, clipboard) == window;
    result("selection-owner", selection_ok,
           selection_ok ? "CLIPBOARD" : last_x_error);
    RECORD(selection_ok);

    XEvent sent = {0};
    sent.xclient.type = ClientMessage;
    sent.xclient.display = display;
    sent.xclient.window = window;
    sent.xclient.message_type = wm_protocols;
    sent.xclient.format = 32;
    sent.xclient.data.l[0] = (long)wm_delete;
    sent.xclient.data.l[1] = CurrentTime;
    before = x_errors;
    Status send_status = XSendEvent(display, window, False, NoEventMask, &sent);
    bool send_ok = send_status && sync_step(display, "client-message-send", before);
    if (!send_status) result("client-message-send", false, "XSendEvent returned 0");
    RECORD(send_ok);

    int keys = 0;
    int key_releases = 0;
    int buttons = 0;
    int motions = 0;
    bool saw_lower_a = false;
    bool saw_shift_press_before_mask = false;
    bool saw_shifted_underscore = false;
    bool saw_shift_release_with_mask = false;
    bool client_message_received = false;
    double deadline = monotonic_seconds() + duration;
    paint(display, window, gc, width, height, passed, failed,
          keys, buttons, motions);
    while (monotonic_seconds() < deadline) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                ++keys;
                KeySym base = XLookupKeysym(&event.xkey, 0);
                KeySym shifted = XLookupKeysym(&event.xkey, 1);
                printf("BXINPUT keycode=%u state=0x%x base=0x%lx shifted=0x%lx\n",
                       event.xkey.keycode, event.xkey.state,
                       (unsigned long)base, (unsigned long)shifted);
                fflush(stdout);
                if (event.xkey.keycode == 38 && event.xkey.state == 0)
                    saw_lower_a = base == XK_a;
                if (event.xkey.keycode == 50 && event.xkey.state == 0)
                    saw_shift_press_before_mask = true;
                if (event.xkey.keycode == 20 &&
                    (event.xkey.state & ShiftMask) != 0)
                    saw_shifted_underscore = shifted == XK_underscore;
            }
            else if (event.type == KeyRelease) {
                ++key_releases;
                printf("BXINPUT release keycode=%u state=0x%x\n",
                       event.xkey.keycode, event.xkey.state);
                fflush(stdout);
                if (event.xkey.keycode == 50 &&
                    (event.xkey.state & ShiftMask) != 0)
                    saw_shift_release_with_mask = true;
            }
            else if (event.type == ButtonPress) ++buttons;
            else if (event.type == MotionNotify) ++motions;
            else if (event.type == ClientMessage &&
                     event.xclient.message_type == wm_protocols &&
                     (Atom)event.xclient.data.l[0] == wm_delete)
                client_message_received = true;
            if (event.type == Expose || event.type == ConfigureNotify ||
                event.type == KeyPress || event.type == ButtonPress ||
                event.type == MotionNotify)
                paint(display, window, gc, width, height, passed, failed,
                      keys, buttons, motions);
        }
        fd_set read_set;
        FD_ZERO(&read_set);
        int fd = ConnectionNumber(display);
        FD_SET(fd, &read_set);
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000};
        if (select(fd + 1, &read_set, NULL, NULL, &timeout) < 0 && errno != EINTR)
            break;
    }

    result("client-message-receive", client_message_received,
           client_message_received ? "WM_DELETE_WINDOW" : "event not delivered");
    RECORD(client_message_received);
    bool modifier_state_ok = saw_lower_a && saw_shift_press_before_mask &&
                             saw_shifted_underscore &&
                             saw_shift_release_with_mask;
    snprintf(detail, sizeof(detail),
             "lower=%d shift-press=%d underscore=%d shift-release=%d releases=%d",
             saw_lower_a, saw_shift_press_before_mask, saw_shifted_underscore,
             saw_shift_release_with_mask, key_releases);
    result("modifier-event-state", modifier_state_ok, detail);
    RECORD(modifier_state_ok);
    snprintf(detail, sizeof(detail), "keys=%d buttons=%d motions=%d",
             keys, buttons, motions);
    printf("BXOBS input-events %s\n", detail);
    printf("BXSUMMARY passed=%d failed=%d observational_input=%s\n",
           passed, failed, keys + buttons + motions > 0 ? "yes" : "no");

    XFreeCursor(display, cursor);
    XFreePixmap(display, cursor_source);
    XDestroyWindow(display, child);
    XFreePixmap(display, pixmap);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 ? 0 : 1;
}
