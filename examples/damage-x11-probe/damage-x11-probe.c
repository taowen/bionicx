#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xmd.h>
#include <X11/Xproto.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t major;
    uint32_t minor;
} dmg_query_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t major;
    uint32_t minor;
    uint32_t pad2[4];
} dmg_query_rep;

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t damage;
    uint32_t drawable;
    uint8_t level;
    uint8_t pad1;
    uint16_t pad2;
} dmg_create_req;

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t damage;
} dmg_destroy_req;

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t damage;
    uint32_t repair;
    uint32_t parts;
} dmg_subtract_req;

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t drawable;
    uint32_t region;
} dmg_add_req;

static void *reserve(Display *display, unsigned bytes) {
    if (display->bufptr + (int)bytes > display->bufmax) _XFlush(display);
    void *ptr = display->bufptr;
    display->last_req = (char *)ptr;
    display->bufptr += bytes;
    display->request++;
    return ptr;
}

static bool query_version(Display *display, int opcode, int *major, int *minor) {
    dmg_query_rep reply;
    LockDisplay(display);
    dmg_query_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 0;
    req->length = 3;
    req->major = 1;
    req->minor = 1;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    *major = (int)reply.major;
    *minor = (int)reply.minor;
    UnlockDisplay(display);
    return true;
}

static void damage_create(Display *display, int opcode, uint32_t damage,
                          uint32_t drawable, uint8_t level) {
    LockDisplay(display);
    dmg_create_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 1;
    req->length = 4;
    req->damage = damage;
    req->drawable = drawable;
    req->level = level;
    req->pad1 = 0;
    req->pad2 = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void damage_destroy(Display *display, int opcode, uint32_t damage) {
    LockDisplay(display);
    dmg_destroy_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 2;
    req->length = 2;
    req->damage = damage;
    UnlockDisplay(display);
    _XFlush(display);
}

static void damage_subtract(Display *display, int opcode, uint32_t damage) {
    LockDisplay(display);
    dmg_subtract_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 3;
    req->length = 4;
    req->damage = damage;
    req->repair = 0;
    req->parts = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void damage_add(Display *display, int opcode, uint32_t drawable) {
    LockDisplay(display);
    dmg_add_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 4;
    req->length = 3;
    req->drawable = drawable;
    req->region = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

/* Xlib's default _XUnknownWireEvent returns False and drops the 32-byte
 * event. Register a converter so DamageNotify reaches XCheckIfEvent. */
static Bool damage_wire_to_event(Display *display, XEvent *re, xEvent *event) {
    re->type = event->u.u.type & 0x7f;
    re->xany.serial = _XSetLastRequestRead(display, (xGenericReply *)event);
    re->xany.send_event = (event->u.u.type & 0x80) != 0;
    re->xany.display = display;
    re->xany.window = event->u.clientMessage.window;
    return True;
}

struct wait_match {
    Window window;
    int type;
};

static Bool match_type(Display *display, XEvent *event, XPointer arg) {
    const struct wait_match *match = (const struct wait_match *)arg;
    (void)display;
    return (event->type & 0x7f) == match->type;
}

static bool wait_damage(Display *display, int type, XEvent *event) {
    struct wait_match match = {.window = 0, .type = type};
    XSync(display, False);
    if (XCheckIfEvent(display, event, match_type, (XPointer)&match))
        return true;
    for (int i = 0; i < 20; ++i) {
        nanosleep(&(struct timespec){.tv_nsec = 10000000L}, NULL);
        XSync(display, False);
        if (XCheckIfEvent(display, event, match_type, (XPointer)&match))
            return true;
    }
    return false;
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
    if (!XQueryExtension(display, "DAMAGE", &opcode, &event_base, &error_base)
            || opcode == 0 || event_base <= 0) {
        fprintf(stderr, "BXFAIL DAMAGE unavailable\n");
        XCloseDisplay(display);
        XCloseDisplay(peer);
        return 2;
    }
    XESetWireToEvent(display, event_base, damage_wire_to_event);

    Window root = DefaultRootWindow(display);
    Window window = XCreateSimpleWindow(display, root, 40, 40, 160, 100, 0,
                                        0, 0x224466);
    XSelectInput(display, window, PropertyChangeMask);
    XMapWindow(display, window);
    XSync(display, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    int major = 0;
    int minor = 0;
    bool version_ok = query_version(display, opcode, &major, &minor)
            && major >= 1 && x_errors == before;
    char version_detail[64];
    if (version_ok)
        snprintf(version_detail, sizeof(version_detail),
                 "DAMAGE 1.1 event_base=%d", event_base);
    else
        snprintf(version_detail, sizeof(version_detail), "QueryVersion failed");
    result("damage-version", version_ok, version_detail);
    RECORD(version_ok);

    before = x_errors;
    uint32_t damage = (uint32_t)XAllocID(display);
    damage_create(display, opcode, damage, (uint32_t)window, 3);
    XSync(display, False);
    bool create_ok = x_errors == before;
    result("damage-create", create_ok,
           create_ok ? "Create ReportNonEmpty" : "Create failed");
    RECORD(create_ok);

    before = x_errors;
    damage_add(display, opcode, (uint32_t)window);
    XEvent event;
    bool notify_ok = wait_damage(display, event_base, &event)
            && x_errors == before;
    char notify_detail[80];
    if (notify_ok) {
        snprintf(notify_detail, sizeof(notify_detail), "DamageNotify from Add");
    } else {
        int pending = XPending(display);
        int seen = -1;
        if (pending > 0) {
            XEvent peek;
            XPeekEvent(display, &peek);
            seen = peek.type & 0x7f;
        }
        snprintf(notify_detail, sizeof(notify_detail),
                 "no DamageNotify pending=%d seen=%d want=%d",
                 pending, seen, event_base);
    }
    result("damage-notify", notify_ok, notify_detail);
    RECORD(notify_ok);

    before = x_errors;
    damage_subtract(display, opcode, damage);
    XSync(display, False);
    bool subtract_ok = x_errors == before;
    result("damage-subtract", subtract_ok,
           subtract_ok ? "Subtract" : "Subtract failed");
    RECORD(subtract_ok);

    before = x_errors;
    damage_destroy(display, opcode, damage);
    XSync(display, False);
    bool destroy_ok = x_errors == before;
    result("damage-destroy", destroy_ok,
           destroy_ok ? "Destroy" : "Destroy failed");
    RECORD(destroy_ok);

    before = x_errors;
    uint32_t grab_damage = (uint32_t)XAllocID(display);
    XGrabServer(display);
    damage_create(display, opcode, grab_damage, (uint32_t)window, 0);
    damage_add(display, opcode, (uint32_t)window);
    bool grab_notify = wait_damage(display, event_base, &event);
    damage_subtract(display, opcode, grab_damage);
    damage_destroy(display, opcode, grab_damage);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = grab_notify && x_errors == before;
    result("damage-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY damage-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
