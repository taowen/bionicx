#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int x_errors;

static int on_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR code=%u request=%u minor=%u resource=0x%lx %s\n",
            event->error_code, event->request_code, event->minor_code,
            event->resourceid, text);
    ++x_errors;
    return 0;
}

static void result(const char *name, bool ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXFAIL open X11 connection\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);
    int opcode = 0;
    int event_base = 0;
    int error_base = 0;
    bool present = XQueryExtension(display, INAME, &opcode, &event_base,
                                   &error_base);
    result("xi1-query", present && opcode != 0,
           present ? "XInputExtension" : "missing");

    int passed = present && opcode != 0 ? 1 : 0;
    int failed = present && opcode != 0 ? 0 : 1;

    int ndevices = 0;
    XDeviceInfo *devices = XListInputDevices(display, &ndevices);
    bool have_pointer = false;
    bool have_keyboard = false;
    if (devices != NULL) {
        for (int i = 0; i < ndevices; ++i) {
            if (devices[i].use == IsXPointer && devices[i].num_classes > 0)
                have_pointer = true;
            if (devices[i].use == IsXKeyboard && devices[i].num_classes > 0)
                have_keyboard = true;
        }
        XFreeDeviceList(devices);
    }
    bool listed = ndevices >= 2 && have_pointer && have_keyboard && x_errors == 0;
    result("xi1-list", listed,
           listed ? "pointer+keyboard" : "ListInputDevices failed");
    if (listed) ++passed;
    else ++failed;

    Window root = DefaultRootWindow(display);
    XEventClass empty = 0;
    XSelectExtensionEvent(display, root, &empty, 0);
    XSync(display, False);
    bool selected = x_errors == 0;
    result("xi1-select", selected,
           selected ? "SelectExtensionEvent count=0"
                    : "SelectExtensionEvent error");
    if (selected) ++passed;
    else ++failed;

    XGrabServer(display);
    ndevices = 0;
    devices = XListInputDevices(display, &ndevices);
    XSelectExtensionEvent(display, root, &empty, 0);
    XUngrabServer(display);
    XSync(display, False);
    bool grabbed = devices != NULL && ndevices >= 2 && x_errors == 0;
    if (devices != NULL) XFreeDeviceList(devices);
    result("xi1-grab-list", grabbed,
           grabbed ? "list under GrabServer" : "blocked or error");
    if (grabbed) ++passed;
    else ++failed;

    printf("BXSUMMARY xi1-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
