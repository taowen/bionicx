#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>

#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

static int passed;
static int failed;

static void check(const char *name, int ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail != NULL && detail[0] != '\0' ? " " : "",
           detail != NULL ? detail : "");
    fflush(stdout);
    if (ok) ++passed;
    else ++failed;
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        printf("BXTEST FAIL display\n");
        printf("BXSUMMARY qt-glx-fbconfig passed=0 failed=1\n");
        return 1;
    }
    int screen = DefaultScreen(display);

    /* Exact qglx_buildSpec() for Krita's first QSurfaceFormat:
     * 3.3 Compatibility, SingleBuffer, rgb=1, alpha/depth/stencil=0.
     * SingleBuffer omits GLX_DOUBLEBUFFER rather than requesting False. */
    int qglx_attribs[] = {
        GLX_LEVEL, 0,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 0,
        GLX_DEPTH_SIZE, 0,
        GLX_STENCIL_SIZE, 0,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        None
    };
    int qglx_count = 0;
    GLXFBConfig *qglx = glXChooseFBConfig(display, screen, qglx_attribs,
                                          &qglx_count);
    char qglx_detail[32];
    snprintf(qglx_detail, sizeof(qglx_detail), "count=%d", qglx_count);
    check("qt-qglx-spec", qglx != NULL && qglx_count > 0, qglx_detail);

    XVisualInfo *visual = NULL;
    if (qglx != NULL && qglx_count > 0)
        visual = glXGetVisualFromFBConfig(display, qglx[0]);
    char visual_detail[48];
    if (visual != NULL)
        snprintf(visual_detail, sizeof(visual_detail),
                 "depth=%d class=%d", visual->depth, visual->class);
    else
        snprintf(visual_detail, sizeof(visual_detail), "null");
    check("qt-qglx-visual", visual != NULL && visual->class == TrueColor,
          visual_detail);

    typedef GLXContext (*create_fn)(Display *, GLXFBConfig, GLXContext, Bool,
                                    const int *);
    create_fn create = (create_fn)glXGetProcAddress(
            (const GLubyte *)"glXCreateContextAttribsARB");
    int context_ok = 0;
    const char *context_detail = "no-config";
    if (create != NULL && qglx != NULL && qglx_count > 0) {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB,
                GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            None
        };
        GLXContext ctx = create(display, qglx[0], NULL, True, attribs);
        context_ok = ctx != NULL;
        context_detail = context_ok ? "gl-3.3" : "create-failed";
        if (ctx != NULL) glXDestroyContext(display, ctx);
    } else if (create == NULL) {
        context_detail = "no-proc";
    }
    check("qt-gl33-context", context_ok, context_detail);

    int current_ok = 0;
    const char *current_detail = "skipped";
    if (context_ok && visual != NULL && qglx != NULL) {
        create_fn create2 = create;
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB,
                GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            None
        };
        GLXContext ctx = create2(display, qglx[0], NULL, True, attribs);
        Colormap colormap = XCreateColormap(display, RootWindow(display, screen),
                                            visual->visual, AllocNone);
        XSetWindowAttributes wa = { .colormap = colormap };
        Window window = XCreateWindow(display, RootWindow(display, screen),
                                      0, 0, 64, 64, 0, visual->depth,
                                      InputOutput, visual->visual,
                                      CWColormap, &wa);
        current_ok = ctx != NULL && window != 0
                && glXMakeCurrent(display, window, ctx);
        current_detail = current_ok ? "current" : "make-current-failed";
        if (ctx != NULL) {
            glXMakeCurrent(display, None, NULL);
            glXDestroyContext(display, ctx);
        }
        if (window != 0) XDestroyWindow(display, window);
        if (colormap != 0) XFreeColormap(display, colormap);
    }
    check("qt-qglx-current", current_ok, current_detail);

    int double_attribs[] = {
        GLX_LEVEL, 0,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DOUBLEBUFFER, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        None
    };
    int double_count = 0;
    GLXFBConfig *dbl = glXChooseFBConfig(display, screen, double_attribs,
                                         &double_count);
    char double_detail[32];
    snprintf(double_detail, sizeof(double_detail), "count=%d", double_count);
    check("qt-double-buffer", dbl != NULL && double_count > 0, double_detail);

    if (visual != NULL) XFree(visual);
    if (qglx != NULL) XFree(qglx);
    if (dbl != NULL) XFree(dbl);
    XCloseDisplay(display);
    printf("BXSUMMARY qt-glx-fbconfig passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
