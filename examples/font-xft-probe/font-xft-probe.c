#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int x_errors;

static int on_x_error(Display *display, XErrorEvent *event) {
    char message[128];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    fprintf(stderr,
            "BXERROR x11 code=%u request=%u minor=%u resource=0x%lx text=%s\n",
            event->error_code, event->request_code, event->minor_code,
            event->resourceid, message);
    ++x_errors;
    return 0;
}

static const char *pattern_string(FcPattern *pattern, const char *key) {
    FcChar8 *value = NULL;
    if (FcPatternGetString(pattern, key, 0, &value) != FcResultMatch)
        return "<missing>";
    return (const char *)value;
}

static void draw_line(XftDraw *draw, XftColor *color, XftFont *font,
                      int x, int y, const char *text) {
    XftDrawStringUtf8(draw, color, font, x, y, (const FcChar8 *)text,
                      (int)strlen(text));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s FONT_DIRECTORY [DURATION_SECONDS]\n", argv[0]);
        return 2;
    }
    int duration = argc >= 3 ? atoi(argv[2]) : 8;
    if (duration < 1 || duration > 60) duration = 8;

    if (!FcInit()) {
        fprintf(stderr, "BXFAIL fontconfig init\n");
        return 3;
    }
    FcConfig *config = FcConfigGetCurrent();
    if (config == NULL ||
            !FcConfigAppFontAddDir(config, (const FcChar8 *)argv[1]) ||
            !FcConfigBuildFonts(config)) {
        fprintf(stderr, "BXFAIL fontconfig add-dir path=%s\n", argv[1]);
        return 4;
    }

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXFAIL XOpenDisplay display=%s\n", getenv("DISPLAY"));
        return 5;
    }
    XSetErrorHandler(on_x_error);
    int screen = DefaultScreen(display);
    XRenderPictFormat *a8 = XRenderFindStandardFormat(display, PictStandardA8);
    if (a8 == NULL) {
        fprintf(stderr, "BXFAIL xrender standard-format=A8 missing\n");
        return 6;
    }

    FcPattern *request = FcNameParse((const FcChar8 *)
            "Liberation Sans:style=Regular:pixelsize=34");
    FcConfigSubstitute(config, request, FcMatchPattern);
    FcDefaultSubstitute(request);
    FcResult match_result = FcResultNoMatch;
    FcPattern *matched = FcFontMatch(config, request, &match_result);
    if (matched == NULL || match_result != FcResultMatch) {
        fprintf(stderr, "BXFAIL fontconfig match result=%d\n", match_result);
        return 7;
    }
    printf("BXFONT match file=%s style=%s a8=0x%lx\n",
            pattern_string(matched, FC_FILE), pattern_string(matched, FC_STYLE),
            a8->id);
    FcPatternDestroy(matched);
    FcPatternDestroy(request);

    XftFont *regular = XftFontOpenName(display, screen,
            "Liberation Sans:style=Regular:pixelsize=34");
    XftFont *bold = XftFontOpenName(display, screen,
            "Liberation Sans:style=Bold:pixelsize=34");
    if (regular == NULL || bold == NULL) {
        fprintf(stderr, "BXFAIL XftFontOpenName regular=%p bold=%p\n",
                (void *)regular, (void *)bold);
        return 8;
    }

    const char *regular_file = pattern_string(regular->pattern, FC_FILE);
    const char *regular_style = pattern_string(regular->pattern, FC_STYLE);
    const char *bold_file = pattern_string(bold->pattern, FC_FILE);
    const char *bold_style = pattern_string(bold->pattern, FC_STYLE);
    if (strcmp(regular_file, bold_file) == 0 ||
            strstr(regular_style, "Regular") == NULL ||
            strstr(bold_style, "Bold") == NULL) {
        fprintf(stderr,
                "BXFAIL face-match regular=%s:%s bold=%s:%s\n",
                regular_file, regular_style, bold_file, bold_style);
        return 9;
    }

    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 90, 90, 1050, 420, 2,
            BlackPixel(display, screen), WhitePixel(display, screen));
    XStoreName(display, window, "BionicX: glibc + Fontconfig + FreeType + Xft");
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);

    XftColor navy = {
        .pixel = 0x00172b4d,
        .color = {.red = 0x1717, .green = 0x2b2b, .blue = 0x4d4d,
                  .alpha = 0xffff}
    };
    XftColor green = {
        .pixel = 0x0000856a,
        .color = {.red = 0x0000, .green = 0x8585, .blue = 0x6a6a,
                  .alpha = 0xffff}
    };
    Visual *visual = DefaultVisual(display, screen);
    Colormap colormap = DefaultColormap(display, screen);
    XftDraw *draw = XftDrawCreate(display, window, visual, colormap);
    if (draw == NULL) {
        fprintf(stderr, "BXFAIL XftDrawCreate\n");
        return 10;
    }

    for (;;) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == Expose && event.xexpose.count == 0) break;
    }
    draw_line(draw, &navy, regular, 55, 95,
              "BionicX regular: real glibc + Xft");
    draw_line(draw, &green, bold, 55, 165,
              "BionicX bold: distinct glyphs 2026");
    draw_line(draw, &navy, regular, 55, 245,
              "Fontconfig selected Liberation Sans Regular");
    draw_line(draw, &navy, bold, 55, 315,
              "Fontconfig selected Liberation Sans Bold");
    XSync(display, False);

    printf("BXFONT regular file=%s style=%s\n", regular_file, regular_style);
    printf("BXFONT bold file=%s style=%s\n", bold_file, bold_style);
    if (x_errors != 0) {
        fprintf(stderr, "BXFAIL Xft rendering x_errors=%d\n", x_errors);
        return 11;
    }
    printf("BXTEST PASS font-xft checks=4/4 display=%s\n",
            DisplayString(display));
    fflush(stdout);
    sleep((unsigned)duration);

    XftDrawDestroy(draw);
    XftFontClose(display, bold);
    XftFontClose(display, regular);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    FcFini();
    return 0;
}
