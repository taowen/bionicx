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
extern void glGetInternalformativ(GLenum target, GLenum internalformat,
        GLenum pname, GLsizei count, GLint *params);
extern void glGetProgramBinary(GLuint program, GLsizei buf_size,
        GLsizei *length, GLenum *binary_format, void *binary);
extern void glProgramBinary(GLuint program, GLenum binary_format,
        const void *binary, GLsizei length);
extern GLuint glCreateShader(GLenum shader_type);
extern GLuint glCreateProgram(void);
extern void glShaderSource(GLuint shader, GLsizei count,
        const GLchar *const *source, const GLint *length);
extern void glCompileShader(GLuint shader);
extern void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
extern void glAttachShader(GLuint program, GLuint shader);
extern void glLinkProgram(GLuint program);
extern void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
extern void glGetProgramInfoLog(GLuint program, GLsizei buf_size,
        GLsizei *length, GLchar *info_log);
extern void glDeleteProgram(GLuint program);
extern void glDeleteShader(GLuint shader);
extern void glGenFramebuffers(GLsizei n, GLuint *framebuffers);
extern void glBindFramebuffer(GLenum target, GLuint framebuffer);
extern void glFramebufferTexture2D(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level);
extern GLenum glCheckFramebufferStatus(GLenum target);
extern void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);

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

    const char *gl_extensions = (const char *)glGetString(GL_EXTENSIONS);
    bool bgra_extension_ok = gl_extensions
            && strstr(gl_extensions,
                      "GL_EXT_texture_format_BGRA8888") != NULL;
    result("host-gl-bgra-texture-extension", bgra_extension_ok,
           bgra_extension_ok ? "GL_EXT_texture_format_BGRA8888"
                             : "extension unavailable");
    bgra_extension_ok ? passed++ : failed++;

    GLint gl_major = 0;
    GLint gl_minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor);
    bool numeric_version_ok = gl_major == 3 && gl_minor == 2;
    snprintf(details, sizeof(details), "version=%d.%d", gl_major, gl_minor);
    result("host-gl-numeric-version", numeric_version_ok, details);
    numeric_version_ok ? passed++ : failed++;

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

    const GLenum required_msaa_formats[] = {
        GL_RGBA8, GL_RGB8, GL_RGB565, GL_RGBA4, GL_RGB5_A1,
        GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH24_STENCIL8,
        GL_STENCIL_INDEX8,
    };
    GLint minimum_max_samples = 0x7fffffff;
    bool required_msaa_ok = true;
    for (size_t i = 0; i < sizeof(required_msaa_formats)
            / sizeof(required_msaa_formats[0]); i++) {
        GLint count = 0;
        GLint samples[32] = {0};
        glGetInternalformativ(GL_RENDERBUFFER, required_msaa_formats[i],
                             GL_NUM_SAMPLE_COUNTS, 1, &count);
        if (count < 1 || count > (GLint)(sizeof(samples) / sizeof(samples[0]))) {
            required_msaa_ok = false;
            minimum_max_samples = 0;
            continue;
        }
        glGetInternalformativ(GL_RENDERBUFFER, required_msaa_formats[i],
                             GL_SAMPLES, count, samples);
        GLint maximum = 0;
        for (GLint j = 0; j < count; j++) {
            if (samples[j] > maximum) maximum = samples[j];
        }
        if (maximum < 4) required_msaa_ok = false;
        if (maximum < minimum_max_samples) minimum_max_samples = maximum;
    }
    snprintf(details, sizeof(details), "formats=%zu minimumMaxSamples=%d",
             sizeof(required_msaa_formats) / sizeof(required_msaa_formats[0]),
             minimum_max_samples);
    result("host-gl-required-format-msaa", required_msaa_ok, details);
    required_msaa_ok ? passed++ : failed++;

    typedef void (*GenTransformFeedbacksProc)(GLsizei, GLuint *);
    typedef void (*BindTransformFeedbackProc)(GLenum, GLuint);
    typedef void (*DeleteTransformFeedbacksProc)(GLsizei, const GLuint *);
    typedef GLboolean (*IsTransformFeedbackProc)(GLuint);
    GenTransformFeedbacksProc gen_transform_feedbacks =
            (GenTransformFeedbacksProc)glXGetProcAddress(
                    (const GLubyte *)"glGenTransformFeedbacks");
    BindTransformFeedbackProc bind_transform_feedback =
            (BindTransformFeedbackProc)glXGetProcAddress(
                    (const GLubyte *)"glBindTransformFeedback");
    DeleteTransformFeedbacksProc delete_transform_feedbacks =
            (DeleteTransformFeedbacksProc)glXGetProcAddress(
                    (const GLubyte *)"glDeleteTransformFeedbacks");
    IsTransformFeedbackProc is_transform_feedback =
            (IsTransformFeedbackProc)glXGetProcAddress(
                    (const GLubyte *)"glIsTransformFeedback");
    GLuint transform_feedback = 0;
    bool transform_feedback_ok = gen_transform_feedbacks
            && bind_transform_feedback && delete_transform_feedbacks
            && is_transform_feedback;
    if (transform_feedback_ok) {
        gen_transform_feedbacks(1, &transform_feedback);
        bool initially_unbound = transform_feedback != 0
                && !is_transform_feedback(transform_feedback);
        bind_transform_feedback(GL_TRANSFORM_FEEDBACK, transform_feedback);
        bool valid_after_bind = is_transform_feedback(transform_feedback);
        bind_transform_feedback(GL_TRANSFORM_FEEDBACK, 0);
        delete_transform_feedbacks(1, &transform_feedback);
        transform_feedback_ok = initially_unbound && valid_after_bind
                && !is_transform_feedback(transform_feedback)
                && glGetError() == GL_NO_ERROR;
    }
    snprintf(details, sizeof(details), "id=%u", transform_feedback);
    result("host-gl-transform-feedback-object", transform_feedback_ok,
           details);
    transform_feedback_ok ? passed++ : failed++;

    const char *vertex_source =
            "#version 120\n"
            "attribute vec4 position;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = position;\n"
            "}\n";
    const char *fragment_source =
            "#version 120\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = vec4(1.0);\n"
            "}\n";
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_source, NULL);
    glShaderSource(fragment_shader, 1, &fragment_source, NULL);
    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);
    GLint vertex_compiled = GL_FALSE;
    GLint fragment_compiled = GL_FALSE;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);
    GLuint source_program = glCreateProgram();
    glAttachShader(source_program, vertex_shader);
    glAttachShader(source_program, fragment_shader);
    glLinkProgram(source_program);
    GLint source_linked = GL_FALSE;
    GLint binary_length = 0;
    glGetProgramiv(source_program, GL_LINK_STATUS, &source_linked);
    glGetProgramiv(source_program, GL_PROGRAM_BINARY_LENGTH, &binary_length);
    if (source_linked != GL_TRUE) {
        GLchar link_log[512] = {0};
        GLsizei link_log_length = 0;
        glGetProgramInfoLog(source_program, sizeof(link_log) - 1,
                            &link_log_length, link_log);
        printf("BXINFO program-link-log %.*s\n", link_log_length, link_log);
    }
    void *program_binary = binary_length > 0
            ? malloc((size_t)binary_length) : NULL;
    GLsizei received_binary_length = 0;
    GLenum binary_format = 0;
    if (program_binary) {
        glGetProgramBinary(source_program, binary_length,
                           &received_binary_length, &binary_format,
                           program_binary);
    }
    GLuint restored_program = glCreateProgram();
    if (received_binary_length > 0) {
        glProgramBinary(restored_program, binary_format, program_binary,
                        received_binary_length);
    }
    GLint restored_linked = GL_FALSE;
    glGetProgramiv(restored_program, GL_LINK_STATUS, &restored_linked);
    bool program_binary_ok = vertex_compiled == GL_TRUE
            && fragment_compiled == GL_TRUE && source_linked == GL_TRUE
            && received_binary_length > 0
            && received_binary_length <= binary_length && binary_format != 0
            && restored_linked == GL_TRUE && glGetError() == GL_NO_ERROR;
    snprintf(details, sizeof(details),
             "sourceLinked=%d length=%d received=%d format=0x%x restored=%d",
             source_linked, binary_length, received_binary_length,
             binary_format, restored_linked);
    result("host-gl-program-binary-roundtrip", program_binary_ok, details);
    program_binary_ok ? passed++ : failed++;
    free(program_binary);
    glDeleteProgram(restored_program);
    glDeleteProgram(source_program);
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    snprintf(details, sizeof(details), "vendor=%s renderer=%s version=%s",
             vendor ? vendor : "-", renderer ? renderer : "-",
             version ? version : "-");
    bool identity_ok = vendor != NULL && renderer != NULL && version != NULL
            && strstr(vendor, "Gladio") != NULL
            && strstr(renderer, "Gladio") != NULL
            && strstr(version, "OpenGL ES 3.") != NULL;
    result("host-gl-identity", identity_ok, details);
    identity_ok ? passed++ : failed++;

    while (glGetError() != GL_NO_ERROR) {}
    GLuint bgra_texture = 0;
    GLuint bgra_framebuffer = 0;
    glGenTextures(1, &bgra_texture);
    glBindTexture(GL_TEXTURE_2D, bgra_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, 8, 8, 0, GL_BGRA,
                 GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &bgra_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, bgra_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, bgra_texture, 0);
    GLenum bgra_framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glViewport(0, 0, 8, 8);
    glClearColor(0.125f, 0.5f, 0.875f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    unsigned char bgra_pixel[4] = {0};
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, bgra_pixel);
    GLenum bgra_error = glGetError();
    bool bgra_render_target_ok =
            bgra_framebuffer_status == GL_FRAMEBUFFER_COMPLETE
            && bgra_error == GL_NO_ERROR
            && bgra_pixel[0] >= 25 && bgra_pixel[0] <= 40
            && bgra_pixel[1] >= 120 && bgra_pixel[1] <= 136
            && bgra_pixel[2] >= 215 && bgra_pixel[2] <= 232
            && bgra_pixel[3] == 255;
    snprintf(details, sizeof(details),
             "status=0x%x pixel=%u,%u,%u,%u glError=0x%x",
             bgra_framebuffer_status, bgra_pixel[0], bgra_pixel[1],
             bgra_pixel[2], bgra_pixel[3], bgra_error);
    result("host-gl-bgra-render-target", bgra_render_target_ok, details);
    bgra_render_target_ok ? passed++ : failed++;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &bgra_framebuffer);
    glDeleteTextures(1, &bgra_texture);

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
        if (now.tv_sec - start.tv_sec >= 20) break;
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
