#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int x_errors;

static int on_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR x11 code=%u request=%u resource=0x%lx text=%s\n",
            event->error_code, event->request_code, event->resourceid, text);
    ++x_errors;
    return 0;
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 4;
    Display *client = XOpenDisplay(NULL);
    Display *manager = XOpenDisplay(NULL);
    if (!client || !manager) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);
    int screen = DefaultScreen(client);
    Window root = RootWindow(client, screen);
    Window app = XCreateSimpleWindow(client, root, 50, 60, 420, 220, 0,
            BlackPixel(client, screen), 0x3264c8);
    XStoreName(client, app, "BionicX save-set client window");
    XMapWindow(client, app);
    XSync(client, False);

    Window manager_root = RootWindow(manager, DefaultScreen(manager));
    Window frame = XCreateSimpleWindow(manager, manager_root, 200, 150,
            500, 300, 0, 0, 0x202020);
    XMapWindow(manager, frame);
    XAddToSaveSet(manager, app);
    XRemoveFromSaveSet(manager, app);
    XAddToSaveSet(manager, app);
    XReparentWindow(manager, app, frame, 20, 25);
    XSync(manager, False);

    Window query_root = None;
    Window parent = None;
    Window *children = NULL;
    unsigned int child_count = 0;
    bool framed = XQueryTree(client, app, &query_root, &parent, &children,
                            &child_count) && parent == frame;
    Window framed_parent = parent;
    if (children) XFree(children);

    XCloseDisplay(manager);
    XSync(client, False);
    children = NULL;
    bool query_ok = XQueryTree(client, app, &query_root, &parent, &children,
                              &child_count);
    if (children) XFree(children);
    XWindowAttributes attributes = {0};
    bool attributes_ok = XGetWindowAttributes(client, app, &attributes);
    XSync(client, False);
    bool restored = query_ok && attributes_ok && parent == root
            && attributes.map_state == IsViewable
            && attributes.x == 220 && attributes.y == 175;

    printf("BXTEST %s save-set-framed parent=0x%lx\n",
            framed ? "PASS" : "FAIL", framed_parent);
    printf("BXTEST %s save-set-manager-disconnect parent=0x%lx geometry=%d,%d map=%d\n",
            restored ? "PASS" : "FAIL", parent, attributes.x, attributes.y,
            attributes.map_state);
    bool passed = framed && restored && x_errors == 0;
    printf("BXSUMMARY save-set-x11 passed=%d/2 xerrors=%d\n",
            passed ? 2 : 0, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 60 ? duration : 4));

    XDestroyWindow(client, app);
    XCloseDisplay(client);
    return passed ? 0 : 1;
}
