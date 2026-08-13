#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <string.h>

static int x_errors;
static char last_x_error[256];

static int on_x_error(Display *display, XErrorEvent *event) {
    char message[128];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    snprintf(last_x_error, sizeof(last_x_error),
             "%s request=%u minor=%u resource=0x%lx", message,
             event->request_code, event->minor_code, event->resourceid);
    ++x_errors;
    return 0;
}

static void result(const char *name, int passed, const char *detail) {
    printf("BXTEST %s %s%s%s\n", passed ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

int main(void) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[256];

    XSetErrorHandler(on_x_error);
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        result("display-connect", 0, "DISPLAY");
        return 1;
    }
    result("display-connect", 1, XDisplayString(display));
    ++passed;

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 40, 40, 360, 200, 0,
                                        BlackPixel(display, screen),
                                        0xff2020);
    XMapWindow(display, window);
    XSync(display, False);
    GC gc = XCreateGC(display, window, 0, NULL);

    int before = x_errors;
    XSetForeground(display, gc, 0xff2020);
    XFillRectangle(display, window, gc, 0, 0, 360, 200);
    XSetForeground(display, gc, 0x20c060);
    XSetBackground(display, gc, 0x2060c0);
    XDrawImageString(display, window, gc, 24, 48, "BionicX ImageText8", 18);
    XImage *image = XGetImage(display, window, 0, 20, 240, 50,
                              AllPlanes, ZPixmap);
    XSync(display, False);
    int field = 0;
    int background = 0;
    int foreground = 0;
    if (image) {
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                unsigned long pixel = XGetPixel(image, x, y) & 0xffffff;
                if (pixel == 0xff2020) ++field;
                else if (pixel == 0x2060c0) ++background;
                else if (pixel != 0) ++foreground;
            }
        }
        XDestroyImage(image);
    }
    int image_ok = x_errors == before && field > 0 && background > 0
            && foreground > 0;
    snprintf(detail, sizeof(detail),
             "field=%d background=%d foreground=%d",
             field, background, foreground);
    result("image-text8", image_ok, image_ok ? detail : last_x_error);
    RECORD(image_ok);

    before = x_errors;
    int event_base = 0;
    int error_base = 0;
    int present = XShapeQueryExtension(display, &event_base, &error_base);
    XSync(display, False);
    int query_ok = x_errors == before && present && event_base > 0;
    snprintf(detail, sizeof(detail), "event=%d error=%d",
             event_base, error_base);
    result("shape-query", query_ok, query_ok ? detail : last_x_error);
    RECORD(query_ok);

    before = x_errors;
    int major = 0;
    int minor = 0;
    XShapeQueryVersion(display, &major, &minor);
    XSync(display, False);
    int version_ok = x_errors == before && major >= 1;
    snprintf(detail, sizeof(detail), "%d.%d", major, minor);
    result("shape-version", version_ok, version_ok ? detail : last_x_error);
    RECORD(version_ok);

    before = x_errors;
    XRectangle rectangle = {.x = 8, .y = 10, .width = 120, .height = 40};
    XShapeCombineRectangles(display, window, ShapeBounding, 4, 6,
                            &rectangle, 1, ShapeSet, Unsorted);
    Bool bounding_shaped = False;
    Bool clip_shaped = False;
    int bx = 0, by = 0, cx = 0, cy = 0;
    unsigned bw = 0, bh = 0, cw = 0, ch = 0;
    XShapeQueryExtents(display, window, &bounding_shaped, &bx, &by, &bw, &bh,
                       &clip_shaped, &cx, &cy, &cw, &ch);
    int count = 0;
    int ordering = 0;
    XRectangle *got = XShapeGetRectangles(display, window, ShapeBounding,
                                          &count, &ordering);
    XSync(display, False);
    int rectangles_ok = x_errors == before && bounding_shaped && count == 1
            && got && got[0].x == 12 && got[0].y == 16
            && got[0].width == 120 && got[0].height == 40
            && bx == 12 && by == 16 && bw == 120 && bh == 40;
    snprintf(detail, sizeof(detail),
             "shaped=%d count=%d rect=%dx%d+%d+%d extents=%ux%u%+d%+d",
             bounding_shaped, count,
             got ? got[0].width : 0, got ? got[0].height : 0,
             got ? got[0].x : -1, got ? got[0].y : -1, bw, bh, bx, by);
    result("shape-rectangles", rectangles_ok,
           rectangles_ok ? detail : last_x_error);
    RECORD(rectangles_ok);
    if (got) XFree(got);

    before = x_errors;
    XShapeSelectInput(display, window, ShapeNotifyMask);
    unsigned long selected = XShapeInputSelected(display, window);
    XSync(display, False);
    int select_ok = x_errors == before && selected == ShapeNotifyMask;
    snprintf(detail, sizeof(detail), "mask=0x%lx", selected);
    result("shape-select-input", select_ok,
           select_ok ? detail : last_x_error);
    RECORD(select_ok);

    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    printf("BXSUMMARY image-text8-shape passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
