#define _POSIX_C_SOURCE 200809L

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

static int passed;
static int failed;
static int x_errors;

static int on_x_error(Display *display, XErrorEvent *event) {
    ++x_errors;
    char message[128];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    fprintf(stderr, "X error: %s request=%u minor=%u\n", message,
            event->request_code, event->minor_code);
    return 0;
}

static void check(const char *name, int ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail != NULL && detail[0] != '\0' ? " " : "",
           detail != NULL ? detail : "");
    fflush(stdout);
    if (ok) ++passed;
    else ++failed;
}

int main(void) {
    void *xtst = dlopen("libXtst.so.6", RTLD_NOW);
    check("xtst-soname", xtst != NULL, xtst != NULL ? "libXtst.so.6" : dlerror());

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        check("display", 0, "DISPLAY");
        printf("BXSUMMARY qt-gui-segv passed=%d failed=%d\n", passed, failed + 1);
        return 1;
    }
    XSetErrorHandler(on_x_error);

    int opcode = 0, event_base = 0, error_base = 0;
    int xi_present = XQueryExtension(display, "XInputExtension",
                                     &opcode, &event_base, &error_base);
    check("xinput-query", xi_present, xi_present ? "XInputExtension" : "missing");

    int xtest_present = XQueryExtension(display, "XTEST",
                                        &opcode, &event_base, &error_base);
    check("xtest-query", xtest_present, xtest_present ? "XTEST" : "missing");

    int major = 0, minor = 0;
    int version_ok = XTestQueryExtension(display, &event_base, &error_base,
                                         &major, &minor);
    char version_detail[32];
    snprintf(version_detail, sizeof(version_detail), "%d.%d", major, minor);
    check("xtest-version", version_ok && major >= 2, version_detail);

    int before = x_errors;
    XTestFakeKeyEvent(display, 38, True, CurrentTime);
    XTestFakeKeyEvent(display, 38, False, CurrentTime);
    XSync(display, False);
    check("xtest-fake-key", x_errors == before, x_errors == before ? "A" : "xerror");

    char plugin_path[512];
    const char *root = getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] == '\0')
        root = "/data/user/0/io.taowen.bx/files/rootfs";
    snprintf(plugin_path, sizeof(plugin_path),
             "%s/usr/lib/aarch64-linux-gnu/keepassxc/libkeepassxc-autotype-xcb.so",
             root);
    void *plugin = dlopen(plugin_path, RTLD_NOW);
    check("autotype-dlopen", plugin != NULL,
          plugin != NULL ? "keepassxc-autotype-xcb" : dlerror());

    int screen = DefaultScreen(display);
    int attribs[] = {
        GLX_LEVEL, 0,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        None
    };
    int count = 0;
    GLXFBConfig *configs = glXChooseFBConfig(display, screen, attribs, &count);
    typedef GLXContext (*create_fn)(Display *, GLXFBConfig, GLXContext, Bool,
                                    const int *);
    create_fn create = (create_fn)glXGetProcAddress(
            (const GLubyte *)"glXCreateContextAttribsARB");
    int draw_ok = 0;
    const char *draw_detail = "no-config";
    if (create != NULL && configs != NULL && count > 0) {
        int ctx_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB,
                GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            None
        };
        GLXContext ctx = create(display, configs[0], NULL, True, ctx_attribs);
        XVisualInfo *visual = glXGetVisualFromFBConfig(display, configs[0]);
        if (ctx != NULL && visual != NULL) {
            Colormap colormap = XCreateColormap(display,
                                                RootWindow(display, screen),
                                                visual->visual, AllocNone);
            XSetWindowAttributes wa = { .colormap = colormap };
            Window window = XCreateWindow(display, RootWindow(display, screen),
                                          0, 0, 64, 64, 0, visual->depth,
                                          InputOutput, visual->visual,
                                          CWColormap, &wa);
            XMapWindow(display, window);
            XSync(display, False);
            if (glXMakeCurrent(display, window, ctx)) {
                (void)glGetError();
                glViewport(0, 0, 64, 64);
                glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                glFlush();
                const GLubyte *version = glGetString(GL_VERSION);
                GLenum err = glGetError();
                if (err == GL_NO_ERROR && version != NULL && version[0] != '\0') {
                    draw_ok = 1;
                    snprintf(version_detail, sizeof(version_detail), "%s",
                             (const char *)version);
                } else {
                    snprintf(version_detail, sizeof(version_detail),
                             "err=0x%x", (unsigned)err);
                }
                draw_detail = version_detail;
            } else {
                draw_detail = "make-current-failed";
            }
            glXMakeCurrent(display, None, NULL);
            glXDestroyContext(display, ctx);
            if (window != 0) XDestroyWindow(display, window);
            if (colormap != 0) XFreeColormap(display, colormap);
        } else {
            draw_detail = "context-or-visual";
            if (ctx != NULL) glXDestroyContext(display, ctx);
        }
        if (visual != NULL) XFree(visual);
    } else if (create == NULL) {
        draw_detail = "no-proc";
    }
    check("qt-draw-after-current", draw_ok, draw_detail);

    if (configs != NULL) XFree(configs);
    if (plugin != NULL) dlclose(plugin);
    if (xtst != NULL) dlclose(xtst);
    XCloseDisplay(display);
    printf("BXSUMMARY qt-gui-segv passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
