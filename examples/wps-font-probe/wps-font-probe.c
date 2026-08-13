#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <string.h>

struct family_check {
    const char *name;
    const char *slug;
    const char *must_contain;
};

/* Latin letters plus the formula operators WPS equation UI checks. */
static const FcChar32 required_glyphs[] = {
    0x41, 0x61, 0x30, 0x00B1, 0x00D7, 0x03C0, 0x221A, 0x2211
};

static int has_required_glyphs(const FcCharSet *charset) {
    for (size_t i = 0; i < sizeof(required_glyphs) / sizeof(required_glyphs[0]);
            ++i) {
        if (!FcCharSetHasChar(charset, required_glyphs[i])) return 0;
    }
    return 1;
}

static int check_family(FcConfig *config, const struct family_check *family) {
    char request[128];
    snprintf(request, sizeof(request), "%s:style=Regular", family->name);
    FcPattern *pattern = FcNameParse((const FcChar8 *)request);
    FcConfigSubstitute(config, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result = FcResultNoMatch;
    FcPattern *matched = FcFontMatch(config, pattern, &result);
    FcChar8 *file = NULL;
    FcChar8 *matched_family = NULL;
    FcCharSet *charset = NULL;
    int have_file = matched != NULL && result == FcResultMatch
            && FcPatternGetString(matched, FC_FILE, 0, &file) == FcResultMatch
            && file != NULL && file[0] != '\0';
    int have_family = matched != NULL
            && FcPatternGetString(matched, FC_FAMILY, 0, &matched_family)
                    == FcResultMatch;
    int have_glyphs = matched != NULL
            && FcPatternGetCharSet(matched, FC_CHARSET, 0, &charset)
                    == FcResultMatch
            && charset != NULL && has_required_glyphs(charset);
    int path_ok = have_file && (family->must_contain == NULL
            || strstr((const char *)file, family->must_contain) != NULL);
    int ok = have_file && have_family && have_glyphs && path_ok;
    printf("BXTEST %s font-family-%s file=%s matched=%s glyphs=%d path=%d\n",
           ok ? "PASS" : "FAIL", family->slug,
           have_file ? (const char *)file : "-",
           have_family ? (const char *)matched_family : "-",
           have_glyphs, path_ok);
    FcPatternDestroy(pattern);
    if (matched) FcPatternDestroy(matched);
    return ok ? 0 : 1;
}

int main(void) {
    static const struct family_check families[] = {
        {"Liberation Sans", "liberation-sans", "LiberationSans"},
        {"Liberation Serif", "liberation-serif", "LiberationSerif"},
        {"Arial", "arial", "LiberationSans"},
        {"Times New Roman", "times-new-roman", "LiberationSerif"},
        {"Calibri", "calibri", "LiberationSans"},
        {"Cambria", "cambria", "LiberationSerif"},
    };

    if (!FcInit()) {
        printf("BXTEST FAIL fontconfig-init\n");
        printf("BXSUMMARY wps-font-families passed=0 failed=1\n");
        return 1;
    }
    FcConfig *config = FcConfigGetCurrent();
    int failed = 0;
    int count = (int)(sizeof(families) / sizeof(families[0]));
    for (int i = 0; i < count; ++i)
        failed += check_family(config, &families[i]);
    printf("BXSUMMARY wps-font-families passed=%d failed=%d\n",
           count - failed, failed);
    return failed ? 1 : 0;
}
