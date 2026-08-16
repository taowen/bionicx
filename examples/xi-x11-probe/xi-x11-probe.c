#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    Display *peer = XOpenDisplay(NULL);
    if (display == NULL || peer == NULL) {
        fprintf(stderr, "BXFAIL open X11 connections\n");
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
    XDevice *device = XOpenDevice(display, 2);
    bool properties_ok = false;
    if (device != NULL) {
        Atom prop = XInternAtom(display, "BionicX Test", False);
        unsigned char value = 7;
        int nprops = -1;
        Atom *listed_props = XListDeviceProperties(display, device, &nprops);
        if (listed_props != NULL) XFree(listed_props);
        XChangeDeviceProperty(display, device, prop, XA_INTEGER, 8,
                              PropModeReplace, &value, 1);
        Atom actual = None;
        int format = 0;
        unsigned long nitems = 0;
        unsigned long after = 0;
        unsigned char *data = NULL;
        int rc = XGetDeviceProperty(display, device, prop, 0, 1, False,
                                    XA_INTEGER, &actual, &format, &nitems,
                                    &after, &data);
        listed_props = XListDeviceProperties(display, device, &nprops);
        bool listed_prop = false;
        if (listed_props != NULL) {
            for (int i = 0; i < nprops; ++i)
                if (listed_props[i] == prop) listed_prop = true;
            XFree(listed_props);
        }
        XDeleteDeviceProperty(display, device, prop);
        XSync(display, False);
        properties_ok = rc == Success && actual == XA_INTEGER && nitems == 1
                && data != NULL && data[0] == 7 && listed_prop
                && x_errors == before;
        if (data != NULL) XFree(data);
    }
    result("xi1-properties", properties_ok,
           properties_ok ? "List/Change/Get/Delete" : "device property failed");
    RECORD(properties_ok);

    before = x_errors;
    bool control_ok = false;
    if (device != NULL) {
        unsigned char map[32] = {0};
        int nmap = XGetDeviceButtonMapping(display, device, map, 32);
        int set_map = MappingSuccess;
        if (nmap > 0)
            set_map = XSetDeviceButtonMapping(display, device, map, nmap);
        int nfeedbacks = 0;
        XFeedbackState *states = XGetFeedbackControl(display, device,
                                                     &nfeedbacks);
        bool have_ptr = false;
        if (states != NULL) {
            XFeedbackState *state = states;
            for (int i = 0; i < nfeedbacks; ++i) {
                if (state->class == PtrFeedbackClass) have_ptr = true;
                state = (XFeedbackState *)((char *)state + state->length);
            }
            XFreeFeedbackList(states);
        }
        int mode_rc = XSetDeviceMode(display, device, Relative);
        XSync(display, False);
        control_ok = nmap >= 3 && map[0] == 1 && set_map == MappingSuccess
                && have_ptr && mode_rc == Success && x_errors == before;
    }
    result("xi1-device-control", control_ok,
           control_ok ? "button/feedback/mode" : "device control failed");
    RECORD(control_ok);

    before = x_errors;
    Status warp = XIWarpPointer(display, 2, None, root, 0, 0, 0, 0, 120, 80);
    Window qroot = None;
    Window qchild = None;
    double root_x = 0;
    double root_y = 0;
    double win_x = 0;
    double win_y = 0;
    XIButtonState buttons = {0};
    XIModifierState modifiers = {0};
    XIGroupState group = {0};
    XIQueryPointer(display, 2, root, &qroot, &qchild, &root_x, &root_y,
                   &win_x, &win_y, &buttons, &modifiers, &group);
    XSync(display, False);
    bool warp_ok = warp == Success && fabs(root_x - 120.0) < 1.0
            && fabs(root_y - 80.0) < 1.0 && x_errors == before;
    char warp_detail[64];
    if (warp_ok) snprintf(warp_detail, sizeof(warp_detail), "XIWarpPointer");
    else snprintf(warp_detail, sizeof(warp_detail), "at %.1f,%.1f",
                  root_x, root_y);
    result("xi2-warp", warp_ok, warp_detail);
    RECORD(warp_ok);
    if (buttons.mask != NULL) free(buttons.mask);

    before = x_errors;
    XISetFocus(display, 3, window, CurrentTime);
    Window focus = None;
    XIGetFocus(display, 3, &focus);
    Window peer_focus = None;
    XIGetFocus(peer, 3, &peer_focus);
    XSync(display, False);
    XSync(peer, False);
    bool focus_ok = focus == window && peer_focus == window
            && x_errors == before;
    result("xi2-focus", focus_ok,
           focus_ok ? "XISet/GetFocus" : "focus failed");
    RECORD(focus_ok);

    unsigned char key_mask_bytes[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(key_mask_bytes, XI_KeyPress);
    XIEventMask key_mask = {
        .deviceid = 3,
        .mask_len = sizeof(key_mask_bytes),
        .mask = key_mask_bytes,
    };
    XISelectEvents(display, window, &key_mask, 1);
    XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
    XIWarpPointer(display, 2, None, window, 0, 0, 0, 0, 100, 60);
    XSync(display, True);
    KeyCode letter = XKeysymToKeycode(peer, XK_a);
    KeyCode control = XKeysymToKeycode(peer, XK_Control_L);
    if (letter != 0 && control != 0) {
        XTestFakeKeyEvent(peer, control, True, 0);
        XFlush(peer);
        usleep(20000);
        XTestFakeKeyEvent(peer, letter, True, 0);
        XFlush(peer);
        usleep(20000);
        XTestFakeKeyEvent(peer, letter, False, 0);
        XTestFakeKeyEvent(peer, control, False, 0);
        XFlush(peer);
    }
    bool key_ok = false;
    int letter_mods = -1;
    for (int i = 0; i < 40 && !key_ok; ++i) {
        if (!XPending(display)) {
            usleep(25000);
            continue;
        }
        XEvent event;
        XNextEvent(display, &event);
        if (event.type != GenericEvent || event.xcookie.extension != opcode)
            continue;
        if (!XGetEventData(display, &event.xcookie)) continue;
        XIDeviceEvent *device_event = event.xcookie.data;
        if (device_event != NULL && device_event->evtype == XI_KeyPress
                && device_event->detail == letter) {
            letter_mods = device_event->mods.effective;
            if ((letter_mods & ControlMask) != 0) key_ok = true;
        }
        XFreeEventData(display, &event.xcookie);
    }
    char key_detail[64];
    if (key_ok) snprintf(key_detail, sizeof(key_detail),
                         "Control+key mods=0x%x", letter_mods);
    else snprintf(key_detail, sizeof(key_detail),
                  "mods=0x%x", letter_mods);
    result("xi2-key-pointer-root", key_ok, key_detail);
    RECORD(key_ok);

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
    if (device != NULL) {
        unsigned char grab_value = 1;
        int grab_nprops = 0;
        Atom grab_prop = XInternAtom(display, "BionicX Grab", False);
        XChangeDeviceProperty(display, device, grab_prop, XA_INTEGER, 8,
                              PropModeReplace, &grab_value, 1);
        Atom *grab_props = XListDeviceProperties(display, device, &grab_nprops);
        if (grab_props != NULL) XFree(grab_props);
        XIWarpPointer(display, 2, None, root, 0, 0, 0, 0, 140, 90);
        XISetFocus(display, 3, window, CurrentTime);
    }
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
    if (device != NULL) XCloseDevice(display, device);
    XDestroyWindow(display, window);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
