#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <stdbool.h>
#include <stdint.h>
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

typedef struct {
    uint8_t reqType;
    uint8_t syncReqType;
    uint16_t length;
    uint8_t major;
    uint8_t minor;
    uint16_t pad;
} sync_init_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint8_t major;
    uint8_t minor;
    uint16_t pad2;
    uint32_t pad3[5];
} sync_init_rep;

typedef struct {
    uint8_t reqType;
    uint8_t syncReqType;
    uint16_t length;
    uint32_t cid;
    int32_t hi;
    uint32_t lo;
} sync_counter_req;

typedef struct {
    uint8_t reqType;
    uint8_t syncReqType;
    uint16_t length;
    uint32_t cid;
} sync_id_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    int32_t hi;
    uint32_t lo;
    uint32_t pad2[4];
} sync_query_rep;

typedef struct {
    uint8_t reqType;
    uint8_t syncReqType;
    uint16_t length;
    uint32_t cid;
    uint32_t value_type;
    int32_t hi;
    uint32_t lo;
    uint32_t test_type;
} sync_await_req;

static void *reserve(Display *display, unsigned bytes) {
    if (display->bufptr + (int)bytes > display->bufmax) _XFlush(display);
    void *ptr = display->bufptr;
    display->last_req = (char *)ptr;
    display->bufptr += bytes;
    display->request++;
    return ptr;
}

static bool initialize(Display *display, int opcode, int *major, int *minor) {
    sync_init_rep reply;
    LockDisplay(display);
    sync_init_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->syncReqType = 0;
    req->length = 2;
    req->major = 3;
    req->minor = 1;
    req->pad = 0;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    *major = reply.major;
    *minor = reply.minor;
    UnlockDisplay(display);
    return true;
}

static void create_counter(Display *display, int opcode, uint32_t cid,
                           int64_t value) {
    LockDisplay(display);
    sync_counter_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->syncReqType = 2;
    req->length = 4;
    req->cid = cid;
    req->hi = (int32_t)(value >> 32);
    req->lo = (uint32_t)value;
    UnlockDisplay(display);
}

static void set_counter(Display *display, int opcode, uint32_t cid,
                        int64_t value) {
    LockDisplay(display);
    sync_counter_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->syncReqType = 3;
    req->length = 4;
    req->cid = cid;
    req->hi = (int32_t)(value >> 32);
    req->lo = (uint32_t)value;
    UnlockDisplay(display);
}

static void change_counter(Display *display, int opcode, uint32_t cid,
                           int64_t amount) {
    LockDisplay(display);
    sync_counter_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->syncReqType = 4;
    req->length = 4;
    req->cid = cid;
    req->hi = (int32_t)(amount >> 32);
    req->lo = (uint32_t)amount;
    UnlockDisplay(display);
}

static bool query_counter(Display *display, int opcode, uint32_t cid,
                          int64_t *value) {
    sync_query_rep reply;
    LockDisplay(display);
    sync_id_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->syncReqType = 5;
    req->length = 2;
    req->cid = cid;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    *value = ((int64_t)reply.hi << 32) | (uint64_t)reply.lo;
    UnlockDisplay(display);
    return true;
}

static void destroy_counter(Display *display, int opcode, uint32_t cid) {
    LockDisplay(display);
    sync_id_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->syncReqType = 6;
    req->length = 2;
    req->cid = cid;
    UnlockDisplay(display);
}

static void await_ge(Display *display, int opcode, uint32_t cid,
                     int64_t wait) {
    LockDisplay(display);
    sync_await_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->syncReqType = 7;
    req->length = 6;
    req->cid = cid;
    req->value_type = 0;
    req->hi = (int32_t)(wait >> 32);
    req->lo = (uint32_t)wait;
    req->test_type = 2;
    UnlockDisplay(display);
}

static void queue_get_input_focus(Display *display) {
    LockDisplay(display);
    xReq *req = reserve(display, 4);
    req->reqType = X_GetInputFocus;
    req->data = 0;
    req->length = 1;
    UnlockDisplay(display);
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
    int major = 0;
    int minor = 0;
    int passed = 0;
    int failed = 0;
    char detail[128];
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    bool present = XQueryExtension(display, "SYNC", &opcode, &event_base,
                                   &error_base) && opcode != 0
            && initialize(display, opcode, &major, &minor)
            && major == 3;
    snprintf(detail, sizeof(detail), "opcode=%d version=%d.%d", opcode,
             major, minor);
    result("sync-initialize", present, detail);
    RECORD(present);
    if (!present) {
        printf("BXSUMMARY sync-x11 passed=%d failed=%d\n", passed, failed);
        XCloseDisplay(peer);
        XCloseDisplay(display);
        return 1;
    }

    uint32_t cid = (uint32_t)XAllocID(display);
    create_counter(display, opcode, cid, 7);
    change_counter(display, opcode, cid, 3);
    set_counter(display, opcode, cid, 10);
    change_counter(display, opcode, cid, 3);
    XSync(display, False);
    int64_t value = 0;
    bool values_ok = query_counter(display, opcode, cid, &value) && value == 13
            && x_errors == 0;
    snprintf(detail, sizeof(detail), "value=%lld", (long long)value);
    result("sync-counter", values_ok, detail);
    RECORD(values_ok);

    int64_t peer_value = 0;
    bool peer_ok = query_counter(peer, opcode, cid, &peer_value)
            && peer_value == 13 && x_errors == 0;
    snprintf(detail, sizeof(detail), "peer=%lld", (long long)peer_value);
    result("sync-peer-query", peer_ok, detail);
    RECORD(peer_ok);

    await_ge(display, opcode, cid, 13);
    XSync(display, False);
    bool ready_ok = x_errors == 0;
    result("sync-await-ready", ready_ok,
           ready_ok ? "already satisfied" : "error");
    RECORD(ready_ok);

    await_ge(peer, opcode, cid, 100);
    queue_get_input_focus(peer);
    _XFlush(peer);
    set_counter(display, opcode, cid, 100);
    XSync(display, False);
    xGetInputFocusReply focus = {0};
    LockDisplay(peer);
    bool await_ok = _XReply(peer, (xReply *)&focus, 0, xTrue) && x_errors == 0;
    UnlockDisplay(peer);
    result("sync-peer-await", await_ok,
           await_ok ? "parked until set" : "no reply");
    RECORD(await_ok);

    uint32_t wake_cid = (uint32_t)XAllocID(display);
    create_counter(display, opcode, wake_cid, 0);
    XSync(display, False);
    await_ge(display, opcode, wake_cid, 1);
    XGrabServer(display);
    queue_get_input_focus(display);
    _XFlush(display);
    set_counter(peer, opcode, wake_cid, 1);
    _XFlush(peer);
    xGetInputFocusReply grab_focus = {0};
    LockDisplay(display);
    bool wake_grab_ok = _XReply(display, (xReply *)&grab_focus, 0, xTrue)
            && x_errors == 0;
    UnlockDisplay(display);
    XUngrabServer(display);
    XSync(display, False);
    result("sync-await-grab", wake_grab_ok,
           wake_grab_ok ? "woke under GrabServer" : "deadlocked");
    RECORD(wake_grab_ok);

    uint32_t grabbed = (uint32_t)XAllocID(display);
    XGrabServer(display);
    create_counter(display, opcode, grabbed, 42);
    XUngrabServer(display);
    XSync(display, False);
    int64_t grabbed_value = 0;
    bool grab_ok = query_counter(peer, opcode, grabbed, &grabbed_value)
            && grabbed_value == 42 && x_errors == 0;
    snprintf(detail, sizeof(detail), "value=%lld", (long long)grabbed_value);
    result("sync-grab", grab_ok, detail);
    RECORD(grab_ok);

    int before = x_errors;
    destroy_counter(display, opcode, cid);
    XSync(display, False);
    int64_t gone = 0;
    bool missing = !query_counter(display, opcode, cid, &gone);
    XSync(display, False);
    bool destroy_ok = missing && x_errors > before;
    result("sync-destroy", destroy_ok,
           destroy_ok ? "BadCounter" : "still queryable");
    RECORD(destroy_ok);

    sleep((unsigned)duration);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    printf("BXSUMMARY sync-x11 passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
