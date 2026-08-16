#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

enum { XSETTINGS_TYPE_INT = 0, XSETTINGS_TYPE_STRING = 1 };

static size_t pad4(size_t n) {
    return (4u - (n % 4u)) % 4u;
}

static void put_u8(unsigned char **p, unsigned char v) {
    *(*p)++ = v;
}

static void put_u16(unsigned char **p, uint16_t v) {
    memcpy(*p, &v, 2);
    *p += 2;
}

static void put_u32(unsigned char **p, uint32_t v) {
    memcpy(*p, &v, 4);
    *p += 4;
}

static void put_pad(unsigned char **p, size_t n) {
    size_t extra = pad4(n);
    memset(*p, 0, extra);
    *p += extra;
}

static void put_name(unsigned char **p, const char *name) {
    uint16_t len = (uint16_t)strlen(name);
    put_u16(p, len);
    memcpy(*p, name, len);
    *p += len;
    put_pad(p, len);
}

/* Typed XSETTINGS body: Xft/DPI integer (dpi*1024) and Gtk/FontName string. */
static size_t encode_settings(unsigned char *out, size_t cap, uint32_t serial,
                              int32_t dpi_xft, const char *font) {
    unsigned char *p = out;
    uint32_t order = 0x01020304;
    put_u8(&p, (*(char *)&order == 1) ? MSBFirst : LSBFirst);
    put_u8(&p, 0);
    put_u8(&p, 0);
    put_u8(&p, 0);
    put_u32(&p, serial);
    put_u32(&p, 2);

    put_u8(&p, XSETTINGS_TYPE_INT);
    put_u8(&p, 0);
    put_name(&p, "Xft/DPI");
    put_u32(&p, serial);
    put_u32(&p, (uint32_t)dpi_xft);

    put_u8(&p, XSETTINGS_TYPE_STRING);
    put_u8(&p, 0);
    put_name(&p, "Gtk/FontName");
    put_u32(&p, serial);
    uint32_t flen = (uint32_t)strlen(font);
    put_u32(&p, flen);
    memcpy(p, font, flen);
    p += flen;
    put_pad(&p, flen);

    size_t n = (size_t)(p - out);
    if (n > cap) return 0;
    return n;
}

static bool get_u8(const unsigned char **p, const unsigned char *end,
                   unsigned char *v) {
    if (*p >= end) return false;
    *v = *(*p)++;
    return true;
}

static bool get_u16(const unsigned char **p, const unsigned char *end,
                    uint16_t *v) {
    if (*p + 2 > end) return false;
    memcpy(v, *p, 2);
    *p += 2;
    return true;
}

static bool get_u32(const unsigned char **p, const unsigned char *end,
                    uint32_t *v) {
    if (*p + 4 > end) return false;
    memcpy(v, *p, 4);
    *p += 4;
    return true;
}

static bool skip_pad(const unsigned char **p, const unsigned char *end,
                     size_t n) {
    size_t extra = pad4(n);
    if (*p + extra > end) return false;
    *p += extra;
    return true;
}

static bool parse_setting(const unsigned char **p, const unsigned char *end,
                          const char *want_name, int want_type,
                          int32_t *int_out, char *str_out, size_t str_cap) {
    unsigned char type = 0, unused = 0;
    uint16_t nlen = 0;
    uint32_t last_serial = 0;
    if (!get_u8(p, end, &type) || !get_u8(p, end, &unused)
            || !get_u16(p, end, &nlen) || *p + nlen > end)
        return false;
    bool match = (int)type == want_type && nlen == strlen(want_name)
            && memcmp(*p, want_name, nlen) == 0;
    *p += nlen;
    if (!skip_pad(p, end, nlen) || !get_u32(p, end, &last_serial))
        return false;
    (void)last_serial;
    if (type == XSETTINGS_TYPE_INT) {
        uint32_t v = 0;
        if (!get_u32(p, end, &v)) return false;
        if (match && int_out != NULL) *int_out = (int32_t)v;
        return match;
    }
    if (type == XSETTINGS_TYPE_STRING) {
        uint32_t slen = 0;
        if (!get_u32(p, end, &slen) || *p + slen > end) return false;
        if (match && str_out != NULL && slen + 1 <= str_cap) {
            memcpy(str_out, *p, slen);
            str_out[slen] = '\0';
        }
        *p += slen;
        if (!skip_pad(p, end, slen)) return false;
        return match;
    }
    return false;
}

static bool parse_settings(const unsigned char *blob, size_t n,
                           int32_t *dpi_out, char *font_out, size_t font_cap,
                           uint32_t *serial_out) {
    const unsigned char *p = blob;
    const unsigned char *end = blob + n;
    unsigned char order = 0, b1 = 0, b2 = 0, b3 = 0;
    uint32_t serial = 0, count = 0;
    if (!get_u8(&p, end, &order) || !get_u8(&p, end, &b1)
            || !get_u8(&p, end, &b2) || !get_u8(&p, end, &b3)
            || !get_u32(&p, end, &serial) || !get_u32(&p, end, &count)
            || count < 2)
        return false;
    (void)order;
    (void)b1;
    (void)b2;
    (void)b3;
    if (serial_out != NULL) *serial_out = serial;
    bool saw_dpi = false, saw_font = false;
    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char *save = p;
        if (parse_setting(&p, end, "Xft/DPI", XSETTINGS_TYPE_INT, dpi_out,
                          NULL, 0))
            saw_dpi = true;
        else {
            p = save;
            if (parse_setting(&p, end, "Gtk/FontName", XSETTINGS_TYPE_STRING,
                              NULL, font_out, font_cap))
                saw_font = true;
            else
                return false;
        }
    }
    return saw_dpi && saw_font;
}

struct wait_match {
    Window window;
    int type;
};

static Bool match_win_type(Display *display, XEvent *event, XPointer arg) {
    const struct wait_match *match = (const struct wait_match *)arg;
    (void)display;
    return event->xany.window == match->window
            && (event->type & 0x7f) == match->type;
}

static bool wait_typed(Display *display, Window window, int type,
                       XEvent *event) {
    struct wait_match match = {.window = window, .type = type};
    XSync(display, False);
    if (XCheckIfEvent(display, event, match_win_type, (XPointer)&match))
        return true;
    for (int i = 0; i < 20; ++i) {
        nanosleep(&(struct timespec){.tv_nsec = 10000000L}, NULL);
        XSync(display, False);
        if (XCheckIfEvent(display, event, match_win_type, (XPointer)&match))
            return true;
    }
    return false;
}

static bool load_settings(Display *display, Window manager, Atom atom,
                          unsigned char **data, unsigned long *nitems) {
    Atom actual = None;
    int format = 0;
    unsigned long after = 0;
    *data = NULL;
    *nitems = 0;
    int rc = XGetWindowProperty(display, manager, atom, 0, 256, False, atom,
                                &actual, &format, nitems, &after, data);
    return rc == Success && actual == atom && format == 8 && *data != NULL
            && *nitems >= 12;
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *manager_dpy = XOpenDisplay(NULL);
    Display *client = XOpenDisplay(NULL);
    if (manager_dpy == NULL || client == NULL) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);

    int screen = DefaultScreen(manager_dpy);
    Window root = RootWindow(manager_dpy, screen);
    Window manager = XCreateSimpleWindow(manager_dpy, root, -1, -1, 1, 1,
                                         0, 0, 0);
    XSelectInput(manager_dpy, manager, PropertyChangeMask);
    XSelectInput(client, manager, PropertyChangeMask);
    XSync(manager_dpy, False);
    XSync(client, False);

    Atom settings_atom = XInternAtom(manager_dpy, "_XSETTINGS_SETTINGS", False);
    Atom selection = XInternAtom(manager_dpy, "_XSETTINGS_S0", False);
    const int32_t dpi_xft = 144 * 1024;
    const char *font = "Sans 11";

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XSetSelectionOwner(manager_dpy, selection, manager, CurrentTime);
    XSync(manager_dpy, False);
    Window owner = XGetSelectionOwner(client, selection);
    bool sel_ok = owner == manager && x_errors == before;
    result("xsettings-typed-selection", sel_ok,
           sel_ok ? "_XSETTINGS_S0" : "selection owner failed");
    RECORD(sel_ok);

    before = x_errors;
    unsigned char blob[256];
    size_t blob_n = encode_settings(blob, sizeof(blob), 1, dpi_xft, font);
    XChangeProperty(manager_dpy, manager, settings_atom, settings_atom, 8,
                    PropModeReplace, blob, (int)blob_n);
    XSync(manager_dpy, False);
    unsigned char *data = NULL;
    unsigned long nitems = 0;
    int32_t dpi = 0;
    char font_got[64];
    font_got[0] = '\0';
    uint32_t serial = 0;
    bool loaded = load_settings(client, manager, settings_atom, &data, &nitems);
    bool parsed = loaded && parse_settings(data, nitems, &dpi, font_got,
                                           sizeof(font_got), &serial);
    bool int_ok = parsed && dpi == dpi_xft && x_errors == before;
    result("xsettings-typed-dpi", int_ok,
           int_ok ? "peer Xft/DPI" : "Xft/DPI missing or wrong");
    RECORD(int_ok);
    bool str_ok = parsed && strcmp(font_got, font) == 0 && x_errors == before;
    result("xsettings-typed-font", str_ok,
           str_ok ? "peer Gtk/FontName" : "Gtk/FontName missing or wrong");
    RECORD(str_ok);
    if (data != NULL) XFree(data);

    before = x_errors;
    XEvent event;
    while (XCheckTypedWindowEvent(client, manager, PropertyNotify, &event)) {}
    blob_n = encode_settings(blob, sizeof(blob), 2, dpi_xft, "Sans 12");
    XChangeProperty(manager_dpy, manager, settings_atom, settings_atom, 8,
                    PropModeReplace, blob, (int)blob_n);
    XSync(manager_dpy, False);
    bool notify_ok = wait_typed(client, manager, PropertyNotify, &event)
            && event.xproperty.window == manager
            && event.xproperty.atom == settings_atom
            && x_errors == before;
    result("xsettings-typed-notify", notify_ok,
           notify_ok ? "peer PropertyNotify on manager"
                     : "client missed settings PropertyNotify");
    RECORD(notify_ok);

    before = x_errors;
    data = NULL;
    dpi = 0;
    font_got[0] = '\0';
    serial = 0;
    loaded = load_settings(client, manager, settings_atom, &data, &nitems);
    parsed = loaded && parse_settings(data, nitems, &dpi, font_got,
                                      sizeof(font_got), &serial);
    bool serial_ok = parsed && serial == 2
            && strcmp(font_got, "Sans 12") == 0 && x_errors == before;
    result("xsettings-typed-serial", serial_ok,
           serial_ok ? "serial 2 new font" : "update not visible to peer");
    RECORD(serial_ok);
    if (data != NULL) XFree(data);

    before = x_errors;
    XGrabServer(manager_dpy);
    blob_n = encode_settings(blob, sizeof(blob), 3, 96 * 1024, "Sans 12");
    XChangeProperty(manager_dpy, manager, settings_atom, settings_atom, 8,
                    PropModeReplace, blob, (int)blob_n);
    XUngrabServer(manager_dpy);
    XSync(manager_dpy, False);
    data = NULL;
    dpi = 0;
    font_got[0] = '\0';
    serial = 0;
    loaded = load_settings(client, manager, settings_atom, &data, &nitems);
    parsed = loaded && parse_settings(data, nitems, &dpi, font_got,
                                      sizeof(font_got), &serial);
    bool grab_ok = parsed && serial == 3 && dpi == 96 * 1024
            && x_errors == before;
    result("xsettings-typed-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or dropped under grab");
    RECORD(grab_ok);
    if (data != NULL) XFree(data);

    printf("BXSUMMARY xsettings-typed-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(manager_dpy, manager);
    XCloseDisplay(client);
    XCloseDisplay(manager_dpy);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
