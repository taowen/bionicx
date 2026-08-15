#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
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
    int major = 2;
    int minor = 0;
    if (!XQueryExtension(display, "XInputExtension", &opcode, &event_base,
                         &error_base)
            || XIQueryVersion(display, &major, &minor) != Success) {
        fprintf(stderr, "BXFAIL XI2 unavailable\n");
        XCloseDisplay(display);
        return 2;
    }

    Window root = DefaultRootWindow(display);
    Window window = XCreateSimpleWindow(display, root, 40, 40, 200, 120, 0,
                                        0, 0x336699);
    XMapWindow(display, window);
    XSync(display, False);

    unsigned char mask_bytes[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(mask_bytes, XI_ButtonPress);
    XISetMask(mask_bytes, XI_ButtonRelease);
    XIEventMask mask = {
        .deviceid = 2,
        .mask_len = sizeof(mask_bytes),
        .mask = mask_bytes,
    };
    XIGrabModifiers mods = {.modifiers = XIAnyModifier, .status = -1};

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

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
    RECORD(listed);

    int before = x_errors;
    XEventClass empty = 0;
    XSelectExtensionEvent(display, root, &empty, 0);
    XSync(display, False);
    bool selected = x_errors == before;
    result("xi1-select", selected,
           selected ? "SelectExtensionEvent" : "select failed");
    RECORD(selected);

    before = x_errors;
    int grab = XIGrabDevice(display, 2, window, 1, None,
                            XIGrabModeSync, XIGrabModeSync, True, &mask);
    XIUngrabDevice(display, 2, 1);
    XSync(display, False);
    bool grab_ok = grab == GrabSuccess && x_errors == before;
    result("xi2-grab-sync", grab_ok,
           grab_ok ? "XIGrabModeSync+timestamp" : "XIGrabDevice rejected");
    RECORD(grab_ok);

    before = x_errors;
    XIAllowEvents(display, 2, XIAsyncDevice, 1);
    XIAllowEvents(display, 2, XIReplayDevice, CurrentTime);
    XIAllowEvents(display, 2, XISyncDevice, 1);
    XSync(display, False);
    bool allow_ok = x_errors == before;
    result("xi2-allow-family", allow_ok,
           allow_ok ? "Async/Replay/Sync" : "XIAllowEvents rejected");
    RECORD(allow_ok);

    before = x_errors;
    int failed_mods = XIGrabButton(display, 2, XIAnyButton, window, None,
                                   XIGrabModeSync, XIGrabModeSync, True,
                                   &mask, 1, &mods);
    XIUngrabButton(display, 2, XIAnyButton, window, 1, &mods);
    XSync(display, False);
    bool passive_ok = failed_mods == 0 && x_errors == before;
    result("xi2-passive-sync", passive_ok,
           passive_ok ? "XIGrabButton Sync" : "passive grab rejected");
    RECORD(passive_ok);

    before = x_errors;
    XGrabServer(display);
    ndevices = 0;
    devices = XListInputDevices(display, &ndevices);
    grab = XIGrabDevice(display, 2, window, 1, None,
                        XIGrabModeSync, XIGrabModeSync, False, &mask);
    XIAllowEvents(display, 2, XIAsyncDevice, 1);
    failed_mods = XIGrabButton(display, 2, XIAnyButton, window, None,
                               XIGrabModeSync, XIGrabModeSync, False,
                               &mask, 1, &mods);
    XIUngrabButton(display, 2, XIAnyButton, window, 1, &mods);
    XIUngrabDevice(display, 2, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool family_ok = devices != NULL && ndevices >= 2 && grab == GrabSuccess
            && failed_mods == 0 && x_errors == before;
    if (devices != NULL) XFreeDeviceList(devices);
    result("xi-family-under-server", family_ok,
           family_ok ? "under GrabServer" : "blocked or error");
    RECORD(family_ok);

    printf("BXSUMMARY xi-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
