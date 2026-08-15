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
    if (display == NULL) {
        fprintf(stderr, "BXFAIL open X11 connection\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);

    int event_base = 0;
    int error_base = 0;
    int major = 1;
    int minor = 3;
    if (!XRRQueryExtension(display, &event_base, &error_base)
            || !XRRQueryVersion(display, &major, &minor) || major < 1) {
        fprintf(stderr, "BXFAIL RANDR unavailable\n");
        XCloseDisplay(display);
        return 2;
    }

    Window root = DefaultRootWindow(display);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(display, root);
    if (res == NULL || res->ncrtc < 1 || res->noutput < 1 || res->nmode < 1) {
        fprintf(stderr, "BXFAIL no RandR resources\n");
        if (res != NULL) XRRFreeScreenResources(res);
        XCloseDisplay(display);
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
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
