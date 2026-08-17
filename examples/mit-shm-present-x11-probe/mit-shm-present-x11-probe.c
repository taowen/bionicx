#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

enum { WIDTH = 32, HEIGHT = 32 };
static const unsigned long kBackground = 0x111111;
static const unsigned long kFill = 0xcc3366;

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

static void *reserve(Display *display, unsigned bytes) {
    if (display->bufptr + (int)bytes > display->bufmax) _XFlush(display);
    void *ptr = display->bufptr;
    display->last_req = (char *)ptr;
    display->bufptr += bytes;
    display->request++;
    return ptr;
}

typedef struct __attribute__((packed)) {
    uint8_t reqType;
    uint8_t presentReqType;
    uint16_t length;
    uint32_t major;
    uint32_t minor;
} present_query_req;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t major;
    uint32_t minor;
    uint32_t pad2[4];
} present_query_rep;

typedef struct __attribute__((packed)) {
    uint8_t reqType;
    uint8_t presentReqType;
    uint16_t length;
    uint32_t window;
    uint32_t pixmap;
    uint32_t serial;
    uint32_t valid;
    uint32_t update;
    int16_t xOff;
    int16_t yOff;
    uint32_t targetCrtc;
    uint32_t waitFence;
    uint32_t idleFence;
    uint32_t options;
    uint32_t pad;
    uint64_t targetMsc;
    uint64_t divisor;
    uint64_t remainder;
} present_pixmap_req;

_Static_assert(sizeof(present_query_req) == 12, "Present QueryVersion");
_Static_assert(sizeof(present_pixmap_req) == 72, "PresentPixmap");

static bool present_query(Display *display, int opcode) {
    present_query_rep reply;
    LockDisplay(display);
    present_query_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->presentReqType = 0;
    req->length = 3;
    req->major = 1;
    req->minor = 0;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    UnlockDisplay(display);
    return true;
}

static void present_pixmap(Display *display, int opcode, Window window,
                           Pixmap pixmap) {
    LockDisplay(display);
    present_pixmap_req *req = reserve(display, sizeof(*req));
    memset(req, 0, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->presentReqType = 1;
    req->length = (uint16_t)(sizeof(*req) / 4);
    req->window = (uint32_t)window;
    req->pixmap = (uint32_t)pixmap;
    req->serial = 1;
    UnlockDisplay(display);
}

static bool paint_window(Display *display, Window window, unsigned long color) {
    int screen = DefaultScreen(display);
    char *pixels = calloc((size_t)WIDTH * HEIGHT, 4u);
    if (pixels == NULL) return false;
    XImage *image = XCreateImage(display, DefaultVisual(display, screen),
                                 (unsigned)DefaultDepth(display, screen),
                                 ZPixmap, 0, pixels, WIDTH, HEIGHT, 32, 0);
    GC gc = XCreateGC(display, window, 0, NULL);
    if (image == NULL || gc == None) {
        if (image != NULL) XDestroyImage(image);
        else free(pixels);
        if (gc != None) XFreeGC(display, gc);
        return false;
    }
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            XPutPixel(image, x, y, color);
    XPutImage(display, window, gc, image, 0, 0, 0, 0, WIDTH, HEIGHT);
    XDestroyImage(image);
    XFreeGC(display, gc);
    return true;
}

static bool read_pixel(Display *display, Drawable drawable,
                       unsigned long *pixel) {
    XImage *got = XGetImage(display, drawable, 0, 0, 1, 1, AllPlanes, ZPixmap);
    if (got == NULL) return false;
    *pixel = XGetPixel(got, 0, 0);
    XDestroyImage(got);
    return true;
}

static bool same_rgb(unsigned long left, unsigned long right) {
    return (left & 0xffffff) == (right & 0xffffff);
}

int main(void) {
    int passed = 0;
    int failed = 0;
    char detail[128];

    XSetErrorHandler(on_x_error);
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXFAIL open DISPLAY\n");
        return 2;
    }

    bool shm_ext = XShmQueryExtension(display) == True;
    result("mit-shm-query", shm_ext, NULL);
    shm_ext ? ++passed : ++failed;

    int present_opcode = 0, present_event = 0, present_error = 0;
    bool present_ext = XQueryExtension(display, "Present", &present_opcode,
                                       &present_event, &present_error) == True
            && present_query(display, present_opcode);
    result("present-query", present_ext, NULL);
    present_ext ? ++passed : ++failed;

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        40, 40, WIDTH, HEIGHT, 0,
                                        BlackPixel(display, screen),
                                        WhitePixel(display, screen));
    XSelectInput(display, window, ExposureMask);
    XMapWindow(display, window);
    XSync(display, False);
    bool painted = paint_window(display, window, kBackground);
    XSync(display, False);
    unsigned long pixel = 0;
    bool background_ok = painted && read_pixel(display, window, &pixel)
            && same_rgb(pixel, kBackground);
    snprintf(detail, sizeof(detail), "pixel=0x%lx", pixel);
    result("window-background", background_ok, detail);
    background_ok ? ++passed : ++failed;

    int size = WIDTH * HEIGHT * 4;
    XShmSegmentInfo info = {0};
    info.shmid = shmget(IPC_PRIVATE, (size_t)size, IPC_CREAT | 0600);
    bool got_shm = info.shmid >= 0;
    snprintf(detail, sizeof(detail), "errno=%d", got_shm ? 0 : errno);
    result("shmget", got_shm, detail);
    got_shm ? ++passed : ++failed;

    if (got_shm) {
        info.shmaddr = shmat(info.shmid, NULL, 0);
        info.readOnly = False;
        if (info.shmaddr == (void *)-1) {
            result("shmat", false, strerror(errno));
            ++failed;
            info.shmaddr = NULL;
        } else {
            result("shmat", true, NULL);
            ++passed;
            memset(info.shmaddr, 0, (size_t)size);
            (void)shmctl(info.shmid, IPC_RMID, NULL);
        }
    }

    bool attached = false;
    if (shm_ext && info.shmaddr != NULL) {
        int before = x_errors;
        attached = XShmAttach(display, &info) == True;
        XSync(display, False);
        attached = attached && x_errors == before;
        result("shm-attach", attached, NULL);
        attached ? ++passed : ++failed;
    } else {
        result("shm-attach", false, "skipped");
        ++failed;
    }

    Pixmap pixmap = None;
    if (attached) {
        char *copy = malloc((size_t)size);
        XImage *fill = XCreateImage(display, DefaultVisual(display, screen),
                                    (unsigned)DefaultDepth(display, screen),
                                    ZPixmap, 0, copy, WIDTH, HEIGHT, 32, 0);
        if (fill != NULL) {
            for (int y = 0; y < HEIGHT; ++y)
                for (int x = 0; x < WIDTH; ++x)
                    XPutPixel(fill, x, y, kFill);
            memcpy(info.shmaddr, fill->data, (size_t)size);
            XDestroyImage(fill);
        } else {
            free(copy);
        }
        int before = x_errors;
        pixmap = XShmCreatePixmap(display, window, info.shmaddr, &info,
                                  WIDTH, HEIGHT,
                                  (unsigned)DefaultDepth(display, screen));
        XSync(display, False);
        Window root = 0;
        int x = 0, y = 0;
        unsigned int width = 0, height = 0, border = 0, depth = 0;
        bool geometry = pixmap != None && x_errors == before
                && XGetGeometry(display, pixmap, &root, &x, &y, &width,
                                &height, &border, &depth) != 0
                && width == WIDTH && height == HEIGHT;
        snprintf(detail, sizeof(detail), "pixmap=0x%lx errors=%d %ux%u",
                 (unsigned long)pixmap, x_errors - before, width, height);
        result("shm-create-pixmap", geometry, detail);
        geometry ? ++passed : ++failed;
    } else {
        result("shm-create-pixmap", false, "skipped");
        ++failed;
    }

    if (present_ext && pixmap != None) {
        int before = x_errors;
        present_pixmap(display, present_opcode, window, pixmap);
        XSync(display, False);
        pixel = 0;
        bool presented = x_errors == before
                && read_pixel(display, window, &pixel)
                && same_rgb(pixel, kFill);
        snprintf(detail, sizeof(detail), "pixel=0x%lx want=0x%lx errors=%d",
                 pixel, kFill, x_errors - before);
        result("shm-present", presented, detail);
        presented ? ++passed : ++failed;
    } else {
        result("shm-present", false, "skipped");
        ++failed;
    }

    if (attached) XShmDetach(display, &info);
    if (pixmap != None) XFreePixmap(display, pixmap);
    if (info.shmaddr != NULL) shmdt(info.shmaddr);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    printf("BXSUMMARY mit-shm-present-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
