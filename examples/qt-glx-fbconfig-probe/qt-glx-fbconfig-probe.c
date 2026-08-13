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

    int listed = 0;
    GLXFBConfig *all = glXGetFBConfigs(display, screen, &listed);
    int found_single = 0;
    int found_double = 0;
    for (int i = 0; i < listed; i++) {
        int db = -1;
        if (all == NULL) break;
        if (glXGetFBConfigAttrib(display, all[i], GLX_DOUBLEBUFFER, &db) != 0)
            continue;
        if (db == 0) found_single = 1;
        if (db == 1) found_double = 1;
    }
    char listed_detail[48];
    snprintf(listed_detail, sizeof(listed_detail),
             "configs=%d single=%d double=%d",
             listed, found_single, found_double);
    check("qt-single-buffer", found_single, listed_detail);
    if (all != NULL) XFree(all);

    int single_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DOUBLEBUFFER, False,
        None
    };
    int single_count = 0;
    GLXFBConfig *single = glXChooseFBConfig(display, screen, single_attribs,
                                            &single_count);

    int double_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DOUBLEBUFFER, True,
        None
    };
    int double_count = 0;
    GLXFBConfig *dbl = glXChooseFBConfig(display, screen, double_attribs,
                                         &double_count);
    char double_detail[32];
    snprintf(double_detail, sizeof(double_detail), "count=%d", double_count);
    check("qt-double-buffer", dbl != NULL && double_count > 0, double_detail);

    typedef GLXContext (*create_fn)(Display *, GLXFBConfig, GLXContext, Bool,
                                    const int *);
    create_fn create = (create_fn)glXGetProcAddress(
            (const GLubyte *)"glXCreateContextAttribsARB");
    int context_ok = 0;
    const char *context_detail = "no-config";
    GLXFBConfig chosen = NULL;
    if (single_count > 0 && single != NULL) chosen = single[0];
    else if (double_count > 0 && dbl != NULL) chosen = dbl[0];
    if (create != NULL && chosen != NULL) {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB,
                GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            None
        };
        GLXContext ctx = create(display, chosen, NULL, True, attribs);
        context_ok = ctx != NULL;
        context_detail = context_ok ? "gl-3.3" : "create-failed";
        if (ctx != NULL) glXDestroyContext(display, ctx);
    } else if (create == NULL) {
        context_detail = "no-proc";
    }
    check("qt-gl33-context", context_ok, context_detail);

    if (single != NULL) XFree(single);
    if (dbl != NULL) XFree(dbl);
    XCloseDisplay(display);
    printf("BXSUMMARY qt-glx-fbconfig passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
