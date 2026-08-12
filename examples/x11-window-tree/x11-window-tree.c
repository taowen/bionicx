#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int x_error_count;

static int handle_x_error(Display *display, XErrorEvent *event) {
    char message[128];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    fprintf(stderr,
            "BXWINDOW_ERROR code=%u request=%u minor=%u resource=0x%" PRIx64
            " text=%s\n",
            event->error_code, event->request_code, event->minor_code,
            (uint64_t)event->resourceid, message);
    x_error_count++;
    return 0;
}

static char *read_text_property(Display *display, Window window, Atom atom) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char *value = NULL;

    if (XGetWindowProperty(display, window, atom, 0, 4096, False,
                           AnyPropertyType, &actual_type, &actual_format,
                           &count, &remaining, &value) != Success ||
        value == NULL || actual_format != 8) {
        if (value != NULL) {
            XFree(value);
        }
        return NULL;
    }

    char *result = calloc(count + 1, 1);
    if (result != NULL) {
        memcpy(result, value, count);
    }
    XFree(value);
    return result;
}

static const char *map_state_name(int state) {
    switch (state) {
        case IsUnmapped: return "unmapped";
        case IsUnviewable: return "unviewable";
        case IsViewable: return "viewable";
        default: return "unknown";
    }
}

static unsigned int print_tree(Display *display, Window window, Atom net_wm_name,
                               unsigned int depth) {
    Window root = None;
    Window parent = None;
    Window *children = NULL;
    unsigned int child_count = 0;
    unsigned int total = 0;

    if (!XQueryTree(display, window, &root, &parent, &children, &child_count)) {
        return 0;
    }

    for (unsigned int i = 0; i < child_count; ++i) {
        XWindowAttributes attributes;
        XClassHint class_hint = {0};
        char *title = read_text_property(display, children[i], net_wm_name);
        if (title == NULL) {
            title = read_text_property(display, children[i], XA_WM_NAME);
        }
        const Status have_attributes =
            XGetWindowAttributes(display, children[i], &attributes);
        const Status have_class = XGetClassHint(display, children[i], &class_hint);
        unsigned long center_pixel = 0;
        char center[32] = "-";

        if (have_attributes && attributes.class == InputOutput &&
            attributes.map_state == IsViewable && attributes.width > 0 &&
            attributes.height > 0) {
            XImage *image = XGetImage(display, children[i],
                                     attributes.width / 2,
                                     attributes.height / 2, 1, 1,
                                     AllPlanes, ZPixmap);
            if (image != NULL) {
                center_pixel = XGetPixel(image, 0, 0);
                snprintf(center, sizeof(center), "0x%08lx", center_pixel);
                XDestroyImage(image);
            }
        }

        if (have_attributes) {
            printf("BXWINDOW depth=%u id=0x%" PRIx64
                   " map=%s override=%d geometry=%dx%d%+d%+d"
                   " center=%s title=%s class=%s/%s\n",
                   depth, (uint64_t)children[i],
                   map_state_name(attributes.map_state),
                   attributes.override_redirect,
                   attributes.width, attributes.height,
                   attributes.x, attributes.y,
                   center,
                   title != NULL ? title : "-",
                   have_class && class_hint.res_name != NULL
                       ? class_hint.res_name : "-",
                   have_class && class_hint.res_class != NULL
                       ? class_hint.res_class : "-");
            total++;
        }

        free(title);
        if (have_class) {
            if (class_hint.res_name != NULL) XFree(class_hint.res_name);
            if (class_hint.res_class != NULL) XFree(class_hint.res_class);
        }
        total += print_tree(display, children[i], net_wm_name, depth + 1);
    }

    if (children != NULL) {
        XFree(children);
    }
    return total;
}

int main(void) {
    XSetErrorHandler(handle_x_error);
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXTEST FAIL window-tree display-open\n");
        return 1;
    }

    Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    Window root = DefaultRootWindow(display);
    unsigned int count = print_tree(display, root, net_wm_name, 0);
    XSync(display, False);
    printf("BXSUMMARY window-tree windows=%u xerrors=%u\n", count,
           x_error_count);
    XCloseDisplay(display);
    return 0;
}
