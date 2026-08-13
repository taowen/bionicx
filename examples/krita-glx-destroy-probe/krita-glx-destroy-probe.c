#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>

/* Krita / Qt xcb-glx calls glXDestroyContext(dpy, NULL) when a
 * QGLXContext create fails or the wrapper is destroyed unused.
 * GLX says a NULL ctx is a no-op. Gladio used to SparseArray_free
 * clientState.vertexArrays at ctx+0x218 and SIGSEGV (fault 0x220). */

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
        printf("BXSUMMARY krita-glx-destroy passed=0 failed=1\n");
        return 1;
    }

    glXDestroyContext(display, NULL);
    check("destroy-null", 1, "no-op");

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
    check("choose-fbconfig", configs != NULL && count > 0, NULL);

    GLXContext created = NULL;
    if (configs != NULL && count > 0)
        created = glXCreateNewContext(display, configs[0], GLX_RGBA_TYPE,
                                      NULL, True);
    check("create-new", created != NULL, NULL);

    if (created != NULL) {
        glXDestroyContext(display, created);
        check("destroy-created", 1, "ok");
    } else {
        check("destroy-created", 0, "no-context");
    }

    if (configs != NULL) XFree(configs);
    XCloseDisplay(display);
    printf("BXSUMMARY krita-glx-destroy passed=%d failed=%d\n",
           passed, failed);
    fflush(stdout);
    return failed ? 1 : 0;
}
