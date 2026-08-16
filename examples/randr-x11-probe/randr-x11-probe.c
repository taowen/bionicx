#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
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
    Display *peer = XOpenDisplay(NULL);
    if (display == NULL || peer == NULL) {
        fprintf(stderr, "BXFAIL open X11 connection\n");
        if (display != NULL) XCloseDisplay(display);
        if (peer != NULL) XCloseDisplay(peer);
        return 2;
    }
    XSetErrorHandler(on_x_error);

    int event_base = 0;
    int error_base = 0;
    int major = 1;
    int minor = 5;
    int peer_major = 1;
    int peer_minor = 5;
    if (!XRRQueryExtension(display, &event_base, &error_base)
            || !XRRQueryVersion(display, &major, &minor) || major < 1
            || minor < 5
            || !XRRQueryExtension(peer, &event_base, &error_base)
            || !XRRQueryVersion(peer, &peer_major, &peer_minor)
            || peer_major < 1 || peer_minor < 5) {
        fprintf(stderr, "BXFAIL RANDR 1.5 unavailable major=%d minor=%d\n",
                major, minor);
        XCloseDisplay(display);
        XCloseDisplay(peer);
        return 2;
    }

    Window root = DefaultRootWindow(display);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(display, root);
    if (res == NULL || res->ncrtc < 1 || res->noutput < 1 || res->nmode < 1) {
        fprintf(stderr, "BXFAIL no RandR resources\n");
        if (res != NULL) XRRFreeScreenResources(res);
        XCloseDisplay(display);
        XCloseDisplay(peer);
        return 2;
    }
    RRCrtc crtc = res->crtcs[0];
    RROutput output = res->outputs[0];
    XRRCrtcInfo *info = XRRGetCrtcInfo(display, res, crtc);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    int min_w = 0, min_h = 0, max_w = 0, max_h = 0;
    XRRGetScreenSizeRange(display, root, &min_w, &min_h, &max_w, &max_h);
    XSync(display, False);
    bool range_ok = min_w > 0 && min_h > 0 && max_w >= min_w && max_h >= min_h
            && x_errors == before;
    result("randr-size-range", range_ok,
           range_ok ? "GetScreenSizeRange" : "size range failed");
    RECORD(range_ok);

    before = x_errors;
    XRRSetOutputPrimary(display, root, output);
    RROutput primary = XRRGetOutputPrimary(display, root);
    XSync(display, False);
    bool primary_ok = primary == output && x_errors == before;
    result("randr-primary", primary_ok,
           primary_ok ? "Set/GetOutputPrimary" : "primary failed");
    RECORD(primary_ok);

    before = x_errors;
    Status config = RRSetConfigFailed;
    if (info != NULL) {
        config = XRRSetCrtcConfig(display, res, crtc, CurrentTime,
                                  info->x, info->y, info->mode,
                                  info->rotation, info->outputs,
                                  info->noutput);
    }
    XSync(display, False);
    bool crtc_ok = info != NULL && config == RRSetConfigSuccess
            && x_errors == before;
    result("randr-crtc-config", crtc_ok,
           crtc_ok ? "SetCrtcConfig Success" : "SetCrtcConfig failed");
    RECORD(crtc_ok);

    before = x_errors;
    XRRCrtcTransformAttributes *transform = NULL;
    Status transform_status = XRRGetCrtcTransform(display, crtc, &transform);
    XTransform scaled = {{{0}}};
    scaled.matrix[0][0] = 0x20000;
    scaled.matrix[1][1] = 0x20000;
    scaled.matrix[2][2] = 0x10000;
    if (transform_status != 0)
        XRRSetCrtcTransform(display, crtc, &scaled, "nearest", NULL, 0);
    if (transform != NULL) {
        XFree(transform);
        transform = NULL;
    }
    transform_status = XRRGetCrtcTransform(display, crtc, &transform);
    XSync(display, False);
    bool transform_ok = transform_status != 0 && transform != NULL
            && transform->currentTransform.matrix[0][0] == 0x20000
            && transform->currentTransform.matrix[1][1] == 0x20000
            && transform->currentTransform.matrix[2][2] == 0x10000
            && x_errors == before;
    result("randr-crtc-transform", transform_ok,
           transform_ok ? "Set/GetCrtcTransform"
                        : "CrtcTransform failed");
    RECORD(transform_ok);
    if (transform != NULL) XFree(transform);

    before = x_errors;
    int gamma_size = XRRGetCrtcGammaSize(display, crtc);
    XRRCrtcGamma *gamma = XRRGetCrtcGamma(display, crtc);
    bool gamma_ok = false;
    if (gamma != NULL && gamma_size > 1) {
        gamma->red[gamma_size - 1] = 0x8000;
        XRRSetCrtcGamma(display, crtc, gamma);
        XRRFreeGamma(gamma);
        gamma = XRRGetCrtcGamma(display, crtc);
        gamma_ok = gamma != NULL && gamma->red[gamma_size - 1] == 0x8000;
    }
    XSync(display, False);
    gamma_ok = gamma_ok && x_errors == before;
    result("randr-gamma", gamma_ok,
           gamma_ok ? "Set/GetCrtcGamma" : "gamma failed");
    RECORD(gamma_ok);
    if (gamma != NULL) XRRFreeGamma(gamma);

    before = x_errors;
    int width = DisplayWidth(display, DefaultScreen(display));
    int height = DisplayHeight(display, DefaultScreen(display));
    int mm_w = DisplayWidthMM(display, DefaultScreen(display));
    int mm_h = DisplayHeightMM(display, DefaultScreen(display));
    XRRSetScreenSize(display, root, width, height, mm_w, mm_h);
    XSync(display, False);
    bool size_ok = x_errors == before;
    result("randr-screen-size", size_ok,
           size_ok ? "SetScreenSize" : "SetScreenSize rejected");
    RECORD(size_ok);

    before = x_errors;
    int nmonitors = 0;
    int peer_nmonitors = 0;
    XRRMonitorInfo *monitors = XRRGetMonitors(display, root, True, &nmonitors);
    XRRMonitorInfo *peer_monitors = XRRGetMonitors(peer,
            DefaultRootWindow(peer), True, &peer_nmonitors);
    XSync(display, False);
    XSync(peer, False);
    bool monitors_ok = monitors != NULL && nmonitors == 1
            && monitors[0].noutput >= 1 && monitors[0].width == width
            && monitors[0].height == height && monitors[0].primary
            && peer_monitors != NULL && peer_nmonitors == 1
            && peer_monitors[0].width == width && x_errors == before;
    result("randr-monitors", monitors_ok,
           monitors_ok ? "GetMonitors" : "GetMonitors failed");
    RECORD(monitors_ok);

    before = x_errors;
    XRROutputInfo *output_info = XRRGetOutputInfo(display, res, output);
    int edid_format = 0;
    Atom edid_type = None;
    unsigned long edid_nitems = 0;
    unsigned long edid_bytes = 0;
    unsigned char *edid_prop = NULL;
    Atom edid_atom = XInternAtom(display, "EDID", False);
    int edid_status = output_info == NULL ? -1
            : XRRGetOutputProperty(display, output, edid_atom, 0, 128,
                                   False, False, AnyPropertyType, &edid_type,
                                   &edid_format, &edid_nitems, &edid_bytes,
                                   &edid_prop);
    int natoms = -1;
    Atom *props = output_info == NULL ? NULL
            : XRRListOutputProperties(display, output, &natoms);
    char *monitor_name = NULL;
    if (monitors != NULL && monitors[0].name != None)
        monitor_name = XGetAtomName(display, monitors[0].name);
    XRRSelectInput(display, root,
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask
                   | RROutputChangeNotifyMask | RROutputPropertyNotifyMask
                   | RRProviderChangeNotifyMask);
    XSync(display, False);
    bool gtk_ok = output_info != NULL
            && output_info->connection == RR_Connected
            && output_info->crtc != None
            && output_info->name != NULL
            && edid_status == Success
            && natoms == 0
            && monitor_name != NULL && monitor_name[0] != '\0'
            && x_errors == before;
    result("randr15-gtk", gtk_ok,
           gtk_ok ? "init_randr15 requests" : "GTK RandR 1.5 path failed");
    RECORD(gtk_ok);
    if (output_info != NULL) XRRFreeOutputInfo(output_info);
    if (edid_prop != NULL) XFree(edid_prop);
    if (props != NULL) XFree(props);
    if (monitor_name != NULL) XFree(monitor_name);
    if (monitors != NULL) XRRFreeMonitors(monitors);
    if (peer_monitors != NULL) XRRFreeMonitors(peer_monitors);

    before = x_errors;
    XGrabServer(display);
    XRRSetOutputPrimary(display, root, output);
    if (info != NULL) {
        XRRSetCrtcConfig(display, res, crtc, CurrentTime, info->x, info->y,
                         info->mode, info->rotation, info->outputs,
                         info->noutput);
    }
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = x_errors == before;
    result("randr-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY randr-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    if (info != NULL) XRRFreeCrtcInfo(info);
    XRRFreeScreenResources(res);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
