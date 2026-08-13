#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int passed;
static int failed;

static void check(const char *name, int ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail != NULL && detail[0] != '\0' ? " " : "",
           detail != NULL ? detail : "");
    fflush(stdout);
    if (ok)
        ++passed;
    else
        ++failed;
}

static int read_file(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "r");
    if (file == NULL)
        return -1;
    size_t n = fread(buffer, 1, size - 1, file);
    fclose(file);
    buffer[n] = '\0';
    return (int)n;
}

int main(void) {
    const char *root = getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/')
        root = "/data/user/0/io.taowen.bx/files/rootfs";
    const char *app = getenv("BIONICX_APP");
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL)
        tmpdir = getenv("BIONICX_TMPDIR");
    if (tmpdir == NULL)
        tmpdir = "/tmp";
    const char *display = getenv("DISPLAY");

    char fixture[512];
    if (app != NULL && app[0] == '/')
        snprintf(fixture, sizeof(fixture), "%s/fixtures/bionicx-writer.odt",
                 app);
    else
        snprintf(fixture, sizeof(fixture),
                 "/data/user/0/io.taowen.bx/files/apps/libreoffice-writer/"
                 "fixtures/bionicx-writer.odt");

    char gen_plugin[512];
    char gtk3_plugin[512];
    char soffice[512];
    snprintf(gen_plugin, sizeof(gen_plugin),
             "%s/usr/lib/libreoffice/program/libvclplug_genlo.so", root);
    snprintf(gtk3_plugin, sizeof(gtk3_plugin),
             "%s/usr/lib/libreoffice/program/libvclplug_gtk3lo.so", root);
    snprintf(soffice, sizeof(soffice),
             "%s/usr/lib/libreoffice/program/soffice.bin", root);

    check("display", display != NULL && display[0] != '\0',
          display != NULL ? display : "missing");
    check("fixture", access(fixture, R_OK) == 0, fixture);
    check("gen-plugin", access(gen_plugin, R_OK) == 0, gen_plugin);
    check("gtk3-plugin", access(gtk3_plugin, R_OK) == 0, gtk3_plugin);
    check("soffice", access(soffice, X_OK) == 0, soffice);

    char swlo[512];
    snprintf(swlo, sizeof(swlo),
             "%s/usr/lib/libreoffice/program/libswlo.so", root);
    void *writer = dlopen(swlo, RTLD_NOW);
    check("dlopen-swlo", writer != NULL, writer != NULL ? swlo : dlerror());

    char outdir[512];
    snprintf(outdir, sizeof(outdir), "%s/soffice-doc-probe", tmpdir);
    if (mkdir(outdir, 0700) != 0 && errno != EEXIST) {
        check("convert-txt", 0, strerror(errno));
        printf("BXSUMMARY soffice-doc passed=%d failed=%d\n", passed, failed);
        return 1;
    }
    char txt[560];
    snprintf(txt, sizeof(txt), "%s/bionicx-writer.txt", outdir);
    unlink(txt);

    /* Document load needs a real VCL. The Writer profile uses gtk3; without
     * libvclplug_gtk3lo.so, soffice.bin throws WrappedTargetRuntimeException
     * as soon as it instantiates a document. */
    char child_log[560];
    snprintf(child_log, sizeof(child_log), "%s/convert.log", outdir);
    unlink(child_log);
    pid_t child = fork();
    if (child == 0) {
        int logfd = open(child_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (logfd >= 0) {
            dup2(logfd, 1);
            dup2(logfd, 2);
            if (logfd > 2)
                close(logfd);
        }
        setenv("DISABLE_EXTENSION_SYNCHRONIZATION", "1", 1);
        char *const args[] = {
            soffice,
            "-env:DISABLE_EXTENSION_SYNCHRONIZATION=1",
            "--headless",
            "--nologo",
            "--nofirststartwizard",
            "--nolockcheck",
            "--norestore",
            "--convert-to",
            "txt:Text",
            "--outdir",
            outdir,
            fixture,
            NULL,
        };
        /* soffice.bin exits 81 to ask the Debian wrapper to restart. */
        for (int attempt = 0; attempt < 3; ++attempt) {
            pid_t inner = fork();
            if (inner == 0)
                execv(soffice, args), _exit(127);
            int inner_status = 0;
            if (inner < 0 || waitpid(inner, &inner_status, 0) < 0)
                _exit(1);
            if (WIFEXITED(inner_status) && WEXITSTATUS(inner_status) == 81)
                continue;
            _exit(WIFEXITED(inner_status) ? WEXITSTATUS(inner_status) : 1);
        }
        _exit(81);
    }
    if (child < 0) {
        check("convert-txt", 0, strerror(errno));
        printf("BXSUMMARY soffice-doc passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    alarm(90);
    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        check("convert-txt", 0, strerror(errno));
        printf("BXSUMMARY soffice-doc passed=%d failed=%d\n", passed, failed);
        return 1;
    }
    alarm(0);

    char detail[128];
    if (WIFSIGNALED(status))
        snprintf(detail, sizeof(detail), "signal=%d", WTERMSIG(status));
    else
        snprintf(detail, sizeof(detail), "exit=%d", WEXITSTATUS(status));
    int convert_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
            access(txt, R_OK) == 0;
    check("convert-txt", convert_ok, detail);
    char logbuf[1024];
    if (read_file(child_log, logbuf, sizeof(logbuf)) > 0)
        printf("BXLOG convert %s\n", logbuf);

    char body[1024];
    int n = convert_ok ? read_file(txt, body, sizeof(body)) : -1;
    int heading = n > 0 && strstr(body, "LibreOffice Writer on BionicX") != NULL;
    check("txt-heading", heading,
          heading ? "LibreOffice Writer on BionicX"
                  : (n > 0 ? body : "missing-txt"));

    printf("BXSUMMARY soffice-doc passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
