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
    uint8_t xresReqType;
    uint16_t length;
    uint8_t major;
    uint8_t minor;
    uint16_t pad;
} xres_version_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t major;
    uint32_t minor;
    uint32_t pad2[4];
} xres_version_rep;

typedef struct {
    uint8_t reqType;
    uint8_t xresReqType;
    uint16_t length;
} xres_header_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t count;
    uint32_t pad2[5];
} xres_count_rep;

typedef struct {
    uint8_t reqType;
    uint8_t xresReqType;
    uint16_t length;
    uint32_t xid;
} xres_xid_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t bytes;
    uint32_t bytes_overflow;
    uint32_t pad2[4];
} xres_bytes_rep;

typedef struct {
    uint32_t resource_base;
    uint32_t resource_mask;
} xres_client;

typedef struct {
    uint32_t type;
    uint32_t count;
} xres_type;

static void *reserve(Display *display, unsigned bytes) {
    if (display->bufptr + (int)bytes > display->bufmax) _XFlush(display);
    void *ptr = display->bufptr;
    display->last_req = (char *)ptr;
    display->bufptr += bytes;
    display->request++;
    return ptr;
}

static bool query_version(Display *display, int opcode, int *major, int *minor) {
    xres_version_rep reply;
    LockDisplay(display);
    xres_version_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->xresReqType = 0;
    req->length = 2;
    req->major = 1;
    req->minor = 2;
    req->pad = 0;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    *major = (int)reply.major;
    *minor = (int)reply.minor;
    UnlockDisplay(display);
    return true;
}

static bool query_clients(Display *display, int opcode, xres_client **out,
                          uint32_t *count) {
    xres_count_rep reply;
    LockDisplay(display);
    xres_header_req *req = reserve(display, 4);
    req->reqType = (uint8_t)opcode;
    req->xresReqType = 1;
    req->length = 1;
    if (!_XReply(display, (xReply *)&reply, 0, xFalse)) {
        UnlockDisplay(display);
        return false;
    }
    *count = reply.count;
    *out = NULL;
    if (reply.count > 0) {
        size_t bytes = (size_t)reply.count * sizeof(xres_client);
        *out = malloc(bytes);
        if (*out == NULL) {
            _XEatDataWords(display, reply.length);
            UnlockDisplay(display);
            return false;
        }
        _XRead(display, (char *)*out, (long)bytes);
    }
    UnlockDisplay(display);
    return true;
}

static bool query_resources(Display *display, int opcode, uint32_t xid,
                            xres_type **out, uint32_t *count) {
    xres_count_rep reply;
    LockDisplay(display);
    xres_xid_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->xresReqType = 2;
    req->length = 2;
    req->xid = xid;
    if (!_XReply(display, (xReply *)&reply, 0, xFalse)) {
        UnlockDisplay(display);
        return false;
    }
    *count = reply.count;
    *out = NULL;
    if (reply.count > 0) {
        size_t bytes = (size_t)reply.count * sizeof(xres_type);
        *out = malloc(bytes);
        if (*out == NULL) {
            _XEatDataWords(display, reply.length);
            UnlockDisplay(display);
            return false;
        }
        _XRead(display, (char *)*out, (long)bytes);
    }
    UnlockDisplay(display);
    return true;
}

static bool query_pixmap_bytes(Display *display, int opcode, uint32_t xid,
                               uint64_t *bytes) {
    xres_bytes_rep reply;
    LockDisplay(display);
    xres_xid_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->xresReqType = 3;
    req->length = 2;
    req->xid = xid;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    *bytes = ((uint64_t)reply.bytes_overflow << 32) | reply.bytes;
    UnlockDisplay(display);
    return true;
}

static bool client_owns(const xres_client *clients, uint32_t count, uint32_t xid) {
    for (uint32_t i = 0; i < count; i++) {
        if ((xid | clients[i].resource_mask)
                == (clients[i].resource_base | clients[i].resource_mask))
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
    int major = 0;
    int minor = 0;
    int passed = 0;
    int failed = 0;
    char detail[128];
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    bool present = XQueryExtension(display, "X-Resource", &opcode, &event_base,
                                   &error_base) && opcode != 0
            && query_version(display, opcode, &major, &minor)
            && major == 1;
    snprintf(detail, sizeof(detail), "opcode=%d version=%d.%d", opcode,
             major, minor);
    result("xres-version", present, detail);
    RECORD(present);
    if (!present) {
        printf("BXSUMMARY xres-x11 passed=%d failed=%d\n", passed, failed);
        XCloseDisplay(peer);
        XCloseDisplay(display);
        return 1;
    }

    Window root = DefaultRootWindow(display);
    Window window = XCreateSimpleWindow(display, root, 20, 20, 64, 48, 0,
                                        0, 0x224466);
    Pixmap pixmap = XCreatePixmap(display, window, 16, 16, 32);
    XSync(display, False);

    xres_client *clients = NULL;
    uint32_t nclients = 0;
    bool listed = query_clients(display, opcode, &clients, &nclients);
    bool clients_ok = listed && nclients >= 2
            && client_owns(clients, nclients, (uint32_t)window)
            && x_errors == 0;
    snprintf(detail, sizeof(detail), "n=%u", nclients);
    result("xres-clients", clients_ok, detail);
    RECORD(clients_ok);
    free(clients);

    uint64_t bytes = 0;
    uint64_t peer_bytes = 0;
    bool bytes_ok = query_pixmap_bytes(display, opcode, (uint32_t)pixmap, &bytes)
            && query_pixmap_bytes(peer, opcode, (uint32_t)pixmap, &peer_bytes)
            && bytes >= 16u * 16u * 4u && bytes == peer_bytes && x_errors == 0;
    snprintf(detail, sizeof(detail), "bytes=%llu peer=%llu",
             (unsigned long long)bytes, (unsigned long long)peer_bytes);
    result("xres-pixmap-bytes", bytes_ok, detail);
    RECORD(bytes_ok);

    xres_type *types = NULL;
    uint32_t ntypes = 0;
    Atom pixmap_atom = XInternAtom(display, "PIXMAP", False);
    bool resources_ok = query_resources(display, opcode, (uint32_t)window,
                                        &types, &ntypes)
            && ntypes >= 1 && types != NULL && types[0].type == pixmap_atom
            && types[0].count >= 1 && x_errors == 0;
    snprintf(detail, sizeof(detail), "n=%u", ntypes);
    result("xres-resources", resources_ok, detail);
    RECORD(resources_ok);
    free(types);

    XGrabServer(display);
    clients = NULL;
    nclients = 0;
    listed = query_clients(display, opcode, &clients, &nclients);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = listed && nclients >= 2 && x_errors == 0;
    snprintf(detail, sizeof(detail), "n=%u", nclients);
    result("xres-grab", grab_ok, detail);
    RECORD(grab_ok);
    free(clients);

    XFreePixmap(display, pixmap);
    XDestroyWindow(display, window);
    sleep((unsigned)duration);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    printf("BXSUMMARY xres-x11 passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
