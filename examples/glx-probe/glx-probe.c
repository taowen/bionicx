#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void result(const char *name, bool passed, const char *detail) {
    printf("BXTEST %s %s %s\n", passed ? "PASS" : "FAIL", name, detail);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    int passed = 0;
    int failed = 0;

    Display *display = XOpenDisplay(NULL);
    bool display_ok = display != NULL;
    result("glx-display", display_ok,
           display_ok ? "connected" : "XOpenDisplay failed");
    display_ok ? passed++ : failed++;
    if (!display_ok) return 1;

    int screen = DefaultScreen(display);
    int visual_attributes[] = {GLX_RGBA, GLX_DOUBLEBUFFER,
                               GLX_DEPTH_SIZE, 16, None};
    XVisualInfo *visual = glXChooseVisual(display, screen, visual_attributes);
    bool visual_ok = visual != NULL;
    result("glx-visual", visual_ok,
           visual_ok ? "RGBA double-buffered" : "unavailable");
    visual_ok ? passed++ : failed++;
    if (!visual_ok) {
        XCloseDisplay(display);
        return 1;
    }

    Colormap colormap = XCreateColormap(display, RootWindow(display, screen),
                                         visual->visual, AllocNone);
    XSetWindowAttributes attributes = {
        .colormap = colormap,
        .event_mask = ExposureMask | StructureNotifyMask,
    };
    const int width = 720;
    const int height = 480;
    Window window = XCreateWindow(display, RootWindow(display, screen),
                                  80, 80, width, height, 0, visual->depth,
                                  InputOutput, visual->visual,
                                  CWColormap | CWEventMask, &attributes);
    XStoreName(display, window, "BionicX host GPU GLX probe");
    XMapWindow(display, window);
    XSync(display, False);

    GLXContext context = glXCreateContext(display, visual, NULL, True);
    bool context_ok = context != NULL
            && glXMakeCurrent(display, window, context);
    result("glx-context", context_ok,
           context_ok ? "current" : "creation/make-current failed");
    context_ok ? passed++ : failed++;
    if (!context_ok) goto cleanup_window;

    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    char details[512];
    snprintf(details, sizeof(details), "vendor=%s renderer=%s version=%s",
             vendor ? vendor : "-", renderer ? renderer : "-",
             version ? version : "-");
    bool identity_ok = vendor != NULL && renderer != NULL && version != NULL;
    result("host-gl-identity", identity_ok, details);
    identity_ok ? passed++ : failed++;

    glViewport(0, 0, width, height);
    glClearColor(0.05f, 0.15f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(0.95f, 0.1f, 0.05f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.8f, -0.8f);
    glVertex2f(0.8f, -0.8f);
    glVertex2f(0.0f, 0.8f);
    glEnd();
    glFinish();

    unsigned char center[4] = {0};
    unsigned char corner[4] = {0};
    glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA,
                 GL_UNSIGNED_BYTE, center);
    glReadPixels(8, height - 8, 1, 1, GL_RGBA,
                 GL_UNSIGNED_BYTE, corner);
    bool pixels_ok = center[0] > 180 && center[1] < 100
            && center[2] < 100 && corner[2] > 140
            && corner[0] < 100;
    snprintf(details, sizeof(details),
             "center=%u,%u,%u,%u corner=%u,%u,%u,%u",
             center[0], center[1], center[2], center[3],
             corner[0], corner[1], corner[2], corner[3]);
    result("host-gl-pixels", pixels_ok, details);
    pixels_ok ? passed++ : failed++;

    glXSwapBuffers(display, window);
    glFinish();
    GLenum error = glGetError();
    bool present_ok = error == GL_NO_ERROR;
    snprintf(details, sizeof(details), "glError=0x%x", error);
    result("glx-present", present_ok, details);
    present_ok ? passed++ : failed++;

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec >= 8) break;
        struct timespec delay = {.tv_nsec = 10 * 1000 * 1000};
        nanosleep(&delay, NULL);
    }

    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, context);

cleanup_window:
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual);
    XCloseDisplay(display);
    printf("BXSUMMARY host-glx passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
