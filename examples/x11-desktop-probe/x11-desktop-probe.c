#define _GNU_SOURCE
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int checks;
static int passed;
static int x_errors;

static int handle_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXXERROR code=%u request=%u minor=%u resource=0x%lx %s\n",
            event->error_code, event->request_code, event->minor_code,
            event->resourceid, text);
    x_errors++;
    return 0;
}

static void result(const char *name, bool ok, const char *detail) {
    checks++;
    if (ok) passed++;
    printf("BXTEST %s %-12s %s\n", ok ? "PASS" : "FAIL", name, detail);
    fflush(stdout);
}

static bool sync_without_error(Display *display, int before) {
    XSync(display, False);
    return x_errors == before;
}

static void probe_render(Display *display, Window window) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XRenderQueryExtension(display, &event_base, &error_base)) {
        result("xrender", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XRenderQueryVersion(display, &major, &minor) != 0;
    XRenderPictFormat *format = XRenderFindVisualFormat(
        display, DefaultVisual(display, DefaultScreen(display)));
    Picture picture = format ? XRenderCreatePicture(display, window, format, 0, NULL) : 0;
    if (picture) {
        XRenderColor color = {.red = 0x1800, .green = 0xb800,
                              .blue = 0xf000, .alpha = 0xffff};
        XRenderFillRectangle(display, PictOpSrc, picture, &color, 24, 84, 300, 92);
        XRenderFreePicture(display, picture);
    }
    ok = ok && format && picture && sync_without_error(display, before);
    XImage *image = ok ? XGetImage(display, window, 100, 120, 1, 1,
                                   AllPlanes, ZPixmap) : NULL;
    ok = ok && image && XGetPixel(image, 0, 0) != 0;
    if (image) XDestroyImage(image);
    char detail[96];
    snprintf(detail, sizeof(detail), "version=%d.%d event=%d error=%d",
             major, minor, event_base, error_base);
    result("xrender", ok, detail);
}

static void probe_xfixes(Display *display) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XFixesQueryExtension(display, &event_base, &error_base)) {
        result("xfixes", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XFixesQueryVersion(display, &major, &minor) != 0;
    XRectangle source = {.x = 7, .y = 9, .width = 31, .height = 37};
    XserverRegion region = XFixesCreateRegion(display, &source, 1);
    int count = 0;
    XRectangle *fetched = XFixesFetchRegion(display, region, &count);
    ok = ok && region && fetched && count == 1 && fetched[0].x == source.x
         && fetched[0].y == source.y && fetched[0].width == source.width
         && fetched[0].height == source.height;
    if (fetched) XFree(fetched);
    if (region) XFixesDestroyRegion(display, region);
    ok = ok && sync_without_error(display, before);
    char detail[96];
    snprintf(detail, sizeof(detail), "version=%d.%d rectangles=%d",
             major, minor, count);
    result("xfixes", ok, detail);
}

static void probe_randr(Display *display, Window root) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XRRQueryExtension(display, &event_base, &error_base)) {
        result("randr", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XRRQueryVersion(display, &major, &minor) != 0;
    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, root);
    RROutput primary = XRRGetOutputPrimary(display, root);
    bool primary_found = false;
    for (int i = 0; resources && i < resources->noutput; i++) {
        if (resources->outputs[i] == primary) primary_found = true;
    }
    ok = ok && resources && resources->ncrtc > 0 && resources->noutput > 0
         && resources->nmode > 0
         && primary != None && primary_found
         && resources->modes[0].width
                == (unsigned int)DisplayWidth(display, DefaultScreen(display))
         && resources->modes[0].height
                == (unsigned int)DisplayHeight(display, DefaultScreen(display))
         && resources->modes[0].nameLength > 0
         && sync_without_error(display, before);
    char detail[160];
    snprintf(detail, sizeof(detail),
             "version=%d.%d crtcs=%d outputs=%d primary=0x%lx mode=%dx%d name=%.*s",
             major, minor, resources ? resources->ncrtc : 0,
             resources ? resources->noutput : 0,
             (unsigned long)primary,
             resources ? resources->modes[0].width : 0,
             resources ? resources->modes[0].height : 0,
             resources ? resources->modes[0].nameLength : 0,
             resources ? resources->modes[0].name : "");
    if (resources) XRRFreeScreenResources(resources);
    result("randr", ok, detail);
}

static void probe_xinput2(Display *display) {
    int opcode = 0, event_base = 0, error_base = 0;
    if (!XQueryExtension(display, "XInputExtension", &opcode,
                         &event_base, &error_base)) {
        result("xinput2", false, "extension-missing");
        return;
    }
    int before = x_errors;
    int major = 2, minor = 0;
    bool ok = XIQueryVersion(display, &major, &minor) == Success;
    int count = 0;
    XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &count);
    bool master_pointer = false, master_keyboard = false;
    for (int i = 0; devices && i < count; i++) {
        master_pointer |= devices[i].use == XIMasterPointer
                          && devices[i].attachment != devices[i].deviceid;
        master_keyboard |= devices[i].use == XIMasterKeyboard
                           && devices[i].attachment != devices[i].deviceid;
    }
    ok = ok && devices && count >= 2 && master_pointer && master_keyboard
         && sync_without_error(display, before);
    if (devices) XIFreeDeviceInfo(devices);
    char detail[96];
    snprintf(detail, sizeof(detail), "version=%d.%d devices=%d masters=%d/%d",
             major, minor, count, master_pointer, master_keyboard);
    result("xinput2", ok, detail);
}

static void probe_xkb(Display *display) {
    int opcode = 0, event_base = 0, error_base = 0;
    int major = XkbMajorVersion, minor = XkbMinorVersion;
    if (!XkbQueryExtension(display, &opcode, &event_base, &error_base,
                           &major, &minor)) {
        result("xkeyboard", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool selected = XkbSelectEvents(display, XkbUseCoreKbd,
                                    XkbStateNotifyMask,
                                    XkbStateNotifyMask);
    XkbDescPtr map = XkbGetMap(display, XkbAllClientInfoMask,
                               XkbUseCoreKbd);
    int map_status = map ? Success : BadImplementation;
    int key_syms = map && map->map && map->map->key_sym_map
                   ? XkbKeyNumSyms(map, 9) : 0;
    int sym_offset = map && map->map && map->map->key_sym_map
                     ? map->map->key_sym_map[9].offset : 0;
    KeySym escape = map && map->map && map->map->key_sym_map
                    ? XkbKeySymEntry(map, 9, 0, 0) : NoSymbol;
    bool ok = selected && map_status == Success && map && map->map
              && map->map->num_types >= 1
              && map->min_key_code <= 9 && map->max_key_code >= 9
              && XkbKeyNumSyms(map, 9) == 1
              && escape == XK_Escape
              && sync_without_error(display, before);
    char detail[160];
    snprintf(detail, sizeof(detail),
             "version=%d.%d opcode=%d selected=%d status=%d keys=%u-%u types=%d syms=%d offset=%d escape=0x%lx",
             major, minor, opcode, selected, map_status,
             map ? map->min_key_code : 0,
             map ? map->max_key_code : 0,
             map && map->map ? map->map->num_types : 0,
             key_syms, sym_offset, (unsigned long)escape);
    if (map) XkbFreeKeyboard(map, XkbAllComponentsMask, True);
    result("xkeyboard", ok, detail);
}

static void probe_optional_shm(Display *display) {
    int major = 0, minor = 0;
    Bool pixmaps = False;
    Bool present = XShmQueryVersion(display, &major, &minor, &pixmaps);
    printf("BXCAP mit-shm %s version=%d.%d pixmaps=%d\n",
           present ? "available" : "unavailable", major, minor, pixmaps);
}

int main(int argc, char **argv) {
    int duration = 8;
    if (argc == 3 && strcmp(argv[1], "--duration") == 0) duration = atoi(argv[2]);
    XSetErrorHandler(handle_x_error);
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "BXTEST FAIL display-open DISPLAY=%s\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(null)");
        return 2;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 80, 80, 720, 420, 0,
                                        BlackPixel(display, screen),
                                        BlackPixel(display, screen));
    XStoreName(display, window, "BionicX X11 desktop extension probe");
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);
    XSync(display, False);

    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, WhitePixel(display, screen));
    const char *title = "BionicX: real glibc desktop X11 extension probe";
    XDrawString(display, window, gc, 24, 42, title, (int)strlen(title));

    probe_render(display, window);
    probe_xfixes(display);
    probe_randr(display, root);
    probe_xinput2(display);
    probe_xkb(display);
    probe_optional_shm(display);

    char summary[128];
    snprintf(summary, sizeof(summary), "desktop extensions: %d/%d strict pass",
             passed, checks);
    XDrawString(display, window, gc, 24, 68, summary, (int)strlen(summary));
    XFlush(display);
    printf("BXSUMMARY desktop-x11 passed=%d failed=%d xerrors=%d\n",
           passed, checks - passed, x_errors);
    fflush(stdout);

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += duration;
    while (true) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
        }
        struct timespec now = {0};
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) break;
        struct timespec pause = {.tv_nsec = 20 * 1000 * 1000};
        nanosleep(&pause, NULL);
    }
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return checks == passed ? 0 : 1;
}
