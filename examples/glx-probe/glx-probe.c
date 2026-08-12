#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern void glGetShaderPrecisionFormat(GLenum shader_type,
        GLenum precision_type, GLint *range, GLint *precision);
extern void glGetInteger64v(GLenum pname, GLint64 *data);
extern void glGetIntegeri_v(GLenum target, GLuint index, GLint *data);

static void result(const char *name, bool passed, const char *detail) {
    printf("BXTEST %s %s %s\n", passed ? "PASS" : "FAIL", name, detail);
}

typedef struct {
    uint8_t major_opcode;
    uint8_t minor_opcode;
    uint16_t length;
    uint32_t screen;
} GLXGetVisualConfigsRequest;

typedef struct {
    uint8_t response_type;
    uint8_t pad;
    uint16_t sequence;
    uint32_t length;
    uint32_t num_visuals;
    uint32_t num_properties;
    uint32_t padding[4];
} GLXGetVisualConfigsReply;

static bool query_visual_config(Display *display, int opcode, int screen,
                                uint32_t *properties, size_t capacity,
                                GLXGetVisualConfigsReply *reply) {
    bool valid = false;
    LockDisplay(display);
    GLXGetVisualConfigsRequest *request =
            _XGetRequest(display, (uint8_t)opcode, sizeof(*request));
    if (request) {
        request->minor_opcode = 14;
        request->screen = (uint32_t)screen;
        if (_XReply(display, (xReply *)reply, 0, False)
                && reply->length <= capacity) {
            _XRead(display, (char *)properties,
                   (long)reply->length * 4L);
            valid = true;
        }
    }
    UnlockDisplay(display);
    return valid;
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
    int glx_opcode = 0;
    int glx_event = 0;
    int glx_error = 0;
    bool extension_ok = glXQueryExtension(display, &glx_error, &glx_event)
            && XQueryExtension(display, "GLX", &glx_opcode, &glx_event,
                               &glx_error);
    char details[512];
    snprintf(details, sizeof(details), "opcode=%d event=%d error=%d",
             glx_opcode, glx_event, glx_error);
    result("glx-extension", extension_ok, details);
    extension_ok ? passed++ : failed++;

    int glx_major = 1;
    int glx_minor = 4;
    bool version_ok = glXQueryVersion(display, &glx_major, &glx_minor)
            && glx_major == 1 && glx_minor >= 4;
    snprintf(details, sizeof(details), "version=%d.%d",
             glx_major, glx_minor);
    result("glx-version", version_ok, details);
    version_ok ? passed++ : failed++;

    uint32_t visual_properties[64] = {0};
    GLXGetVisualConfigsReply visual_reply = {0};
    bool visual_config_ok = extension_ok
            && query_visual_config(display, glx_opcode, screen,
                                   visual_properties,
                                   sizeof(visual_properties)
                                           / sizeof(visual_properties[0]),
                                   &visual_reply)
            && visual_reply.num_visuals == 1
            && visual_reply.num_properties == 18
            && visual_reply.length == 18
            && visual_properties[0] != 0
            && visual_properties[1] == TrueColor
            && visual_properties[2] == 1
            && visual_properties[3] == 8
            && visual_properties[4] == 8
            && visual_properties[5] == 8
            && visual_properties[11] == 1
            && visual_properties[14] == 24;
    snprintf(details, sizeof(details),
             "visuals=%u properties=%u visual=0x%x rgba=%u depth=%u",
             visual_reply.num_visuals, visual_reply.num_properties,
             visual_properties[0], visual_properties[2],
             visual_properties[14]);
    result("glx-visual-config-wire", visual_config_ok, details);
    visual_config_ok ? passed++ : failed++;

    int fbconfig_count = 0;
    GLXFBConfig *fbconfigs = glXGetFBConfigs(display, screen,
                                             &fbconfig_count);
    int fb_visual_id = 0;
    int fb_x_renderable = 0;
    XVisualInfo *fb_visual = fbconfigs && fbconfig_count > 0
            ? glXGetVisualFromFBConfig(display, fbconfigs[0]) : NULL;
    bool fbconfig_ok = fbconfigs && fbconfig_count == 1 && fb_visual
            && glXGetFBConfigAttrib(display, fbconfigs[0], GLX_VISUAL_ID,
                                    &fb_visual_id) == Success
            && glXGetFBConfigAttrib(display, fbconfigs[0], GLX_X_RENDERABLE,
                                    &fb_x_renderable) == Success
            && fb_visual_id == (int)XVisualIDFromVisual(fb_visual->visual)
            && fb_x_renderable == True;
    snprintf(details, sizeof(details),
             "configs=%d visual=0x%x xRenderable=%d",
             fbconfig_count, fb_visual_id, fb_x_renderable);
    result("glx-fbconfig-visual", fbconfig_ok, details);
    fbconfig_ok ? passed++ : failed++;

    const char *extensions = glXQueryExtensionsString(display, screen);
    bool es_profile_ok = extensions
            && strstr(extensions, "GLX_EXT_create_context_es_profile");
    result("glx-es-profile-extension", es_profile_ok,
           extensions ? extensions : "unavailable");
    es_profile_ok ? passed++ : failed++;

    int pbuffer_attributes[] = {
        GLX_PBUFFER_WIDTH, 7,
        GLX_PBUFFER_HEIGHT, 5,
        None
    };
    GLXPbuffer pbuffer = fbconfigs && fbconfig_count > 0
            ? glXCreatePbuffer(display, fbconfigs[0], pbuffer_attributes) : 0;
    unsigned int pbuffer_width = 0;
    unsigned int pbuffer_height = 0;
    if (pbuffer) {
        glXQueryDrawable(display, pbuffer, GLX_WIDTH, &pbuffer_width);
        glXQueryDrawable(display, pbuffer, GLX_HEIGHT, &pbuffer_height);
    }
    bool pbuffer_ok = pbuffer && pbuffer_width == 7 && pbuffer_height == 5;
    snprintf(details, sizeof(details), "drawable=0x%lx size=%ux%u",
             (unsigned long)pbuffer, pbuffer_width, pbuffer_height);
    result("glx-pbuffer", pbuffer_ok, details);
    pbuffer_ok ? passed++ : failed++;
    if (pbuffer) glXDestroyPbuffer(display, pbuffer);
    if (fb_visual) XFree(fb_visual);
    free(fbconfigs);

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

    int rgba = 0;
    int double_buffer = 0;
    int red_size = 0;
    int depth_size = 0;
    int visual_type = 0;
    bool config_ok = glXGetConfig(display, visual, GLX_RGBA, &rgba) == Success
            && glXGetConfig(display, visual, GLX_DOUBLEBUFFER,
                            &double_buffer) == Success
            && glXGetConfig(display, visual, GLX_RED_SIZE, &red_size) == Success
            && glXGetConfig(display, visual, GLX_DEPTH_SIZE,
                            &depth_size) == Success
            && glXGetConfig(display, visual, GLX_X_VISUAL_TYPE,
                            &visual_type) == Success
            && rgba == True && double_buffer == True && red_size == 8
            && depth_size == 24 && visual_type == GLX_TRUE_COLOR;
    snprintf(details, sizeof(details),
             "rgba=%d double=%d red=%d depth=%d type=0x%x",
             rgba, double_buffer, red_size, depth_size, visual_type);
    result("glx-visual-config-api", config_ok, details);
    config_ok ? passed++ : failed++;

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

    typedef const GLubyte *(*GetStringProc)(GLenum);
    GetStringProc get_string = (GetStringProc)glXGetProcAddress(
            (const GLubyte *)"glGetString");
    const char *proc_version = get_string
            ? (const char *)get_string(GL_VERSION) : NULL;
    bool proc_address_ok = proc_version != NULL;
    result("glx-gl-proc-address", proc_address_ok,
           proc_version ? proc_version : "glGetString unavailable");
    proc_address_ok ? passed++ : failed++;

    GLint precision_range[2] = {0};
    GLint precision_bits = 0;
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
                               precision_range, &precision_bits);
    bool precision_ok = precision_range[0] > 0
            && precision_range[1] > 0 && precision_bits > 0;
    snprintf(details, sizeof(details), "range=%d..%d precision=%d",
             precision_range[0], precision_range[1], precision_bits);
    result("host-gl-shader-precision", precision_ok, details);
    precision_ok ? passed++ : failed++;

    GLint64 max_element_index = 0;
    glGetInteger64v(GL_MAX_ELEMENT_INDEX, &max_element_index);
    bool integer64_ok = max_element_index > 0;
    snprintf(details, sizeof(details), "maxElementIndex=%lld",
             (long long)max_element_index);
    result("host-gl-integer64", integer64_ok, details);
    integer64_ok ? passed++ : failed++;

    GLint max_compute_count = 0;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0,
                    &max_compute_count);
    bool indexed_integer_ok = max_compute_count > 0;
    snprintf(details, sizeof(details), "maxComputeWorkGroupsX=%d",
             max_compute_count);
    result("host-gl-indexed-integer", indexed_integer_ok, details);
    indexed_integer_ok ? passed++ : failed++;

    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    snprintf(details, sizeof(details), "vendor=%s renderer=%s version=%s",
             vendor ? vendor : "-", renderer ? renderer : "-",
             version ? version : "-");
    bool identity_ok = vendor != NULL && renderer != NULL && version != NULL
            && strstr(version, "OpenGL ES 3.") != NULL;
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
