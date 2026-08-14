#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int x_errors;
static int last_error;

static int on_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR code=%u request=%u resource=0x%lx %s\n",
            event->error_code, event->request_code, event->resourceid, text);
    ++x_errors;
    last_error = event->error_code;
    return 0;
}

static void result(const char *name, bool ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

static bool get_maxlong(Display *display, Window window, Atom property,
                        Atom type, unsigned long *nitems_out,
                        unsigned char **data_out) {
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long bytes = 0;
    unsigned char *data = NULL;
    int status = XGetWindowProperty(display, window, property, 0L, LONG_MAX,
                                    False, type, &actual, &format, &nitems,
                                    &bytes, &data);
    if (status != Success) {
        if (data != NULL) XFree(data);
        return false;
    }
    *nitems_out = nitems;
    *data_out = data;
    return true;
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXFAIL open X11 connection\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 10, 10, 200, 40, 0, 0,
                                        0x222222);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    Atom missing = XInternAtom(display, "_BIONICX_MISSING_PROP", False);
    unsigned long nitems = 99;
    unsigned char *data = NULL;
    bool missing_ok = get_maxlong(display, window, missing, XA_ATOM, &nitems,
                                  &data)
            && nitems == 0 && data == NULL;
    result("get-missing-maxlong", missing_ok,
           missing_ok ? "nitems=0" : "G_MAXLONG missing property failed");
    RECORD(missing_ok);
    if (data != NULL) XFree(data);

    Atom net_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(display, window, net_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&dock, 1);
    data = NULL;
    nitems = 0;
    bool type_ok = get_maxlong(display, window, net_type, XA_ATOM, &nitems,
                               &data)
            && nitems == 1 && data != NULL
            && *(Atom *)data == dock;
    result("get-type-maxlong", type_ok,
           type_ok ? "TYPE_DOCK" : "G_MAXLONG ATOM list failed");
    RECORD(type_ok);
    if (data != NULL) XFree(data);

    Atom strut = XInternAtom(display, "_NET_WM_STRUT_PARTIAL", False);
    unsigned long struts[12] = {0, 0, 27, 0, 0, 0, 0, 0, 0, 2639, 0, 0};
    XChangeProperty(display, window, strut, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)struts, 12);
    data = NULL;
    nitems = 0;
    bool strut_ok = get_maxlong(display, window, strut, XA_CARDINAL, &nitems,
                                &data)
            && nitems == 12 && data != NULL
            && ((unsigned long *)data)[2] == 27;
    result("get-strut-maxlong", strut_ok,
           strut_ok ? "12 cardinals" : "G_MAXLONG CARDINAL list failed");
    RECORD(strut_ok);
    if (data != NULL) XFree(data);

    last_error = 0;
    char *none_name = XGetAtomName(display, None);
    XSync(display, False);
    bool none_ok = none_name == NULL && last_error == BadAtom;
    result("get-atom-none", none_ok,
           none_ok ? "BadAtom" : "GetAtomName(None) did not error");
    RECORD(none_ok);
    if (none_name != NULL) XFree(none_name);

    last_error = 0;
    char *bogus_name = XGetAtomName(display, (Atom)0x00ffffff);
    XSync(display, False);
    bool bogus_ok = bogus_name == NULL && last_error == BadAtom;
    result("get-atom-unknown", bogus_ok,
           bogus_ok ? "BadAtom" : "GetAtomName(unknown) did not error");
    RECORD(bogus_ok);
    if (bogus_name != NULL) XFree(bogus_name);

    printf("BXSUMMARY get-property-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 ? 0 : 1;
}
