#include <cups/cups.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Symbols KCUPSSupport::resolveCups() binds via QLibrary("cups", 2). */
static const char *const wps_cups_symbols[] = {
    "cupsAddOption",
    "cupsFreeDests",
    "cupsFreeOptions",
    "cupsGetDest",
    "cupsGetDests",
    "cupsGetOption",
    "cupsLastError",
    "cupsLastErrorString",
    "cupsPrintFile",
    "httpClose",
    "httpConnect2",
    "ippAddString",
    "ippDelete",
    "ippFindAttribute",
    "ippNewRequest",
    "ppdClose",
    "ppdFindOption",
    "ppdOpenFile",
};

static int passed;
static int failed;

static void check(const char *name, int ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && detail[0] ? " " : "", detail ? detail : "");
    if (ok)
        passed++;
    else
        failed++;
}

static const char *file_uri_path(const char *uri) {
    if (uri == NULL) return NULL;
    if (strncmp(uri, "file:", 5) != 0) return NULL;
    uri += 5;
    if (uri[0] == '/' && uri[1] == '/') {
        const char *slash = strchr(uri + 2, '/');
        return slash != NULL ? slash : NULL;
    }
    return uri;
}

int main(void) {
    void *handle = dlopen("libcups.so.2", RTLD_NOW);
    check("dlopen-libcups-so-2", handle != NULL,
          handle != NULL ? "QLibrary(cups, 2)" : dlerror());
    if (handle == NULL) {
        printf("BXSUMMARY wps-cups passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    int unresolved = 0;
    for (size_t i = 0; i < sizeof(wps_cups_symbols) / sizeof(wps_cups_symbols[0]);
            ++i) {
        if (dlsym(handle, wps_cups_symbols[i]) == NULL) {
            printf("BXTEST FAIL resolve-%s %s\n", wps_cups_symbols[i],
                   dlerror());
            unresolved++;
            failed++;
        } else {
            printf("BXTEST PASS resolve-%s\n", wps_cups_symbols[i]);
            passed++;
        }
    }
    check("wps-symbol-set", unresolved == 0, "KCUPSSupport resolveCups");

    cups_dest_t *destinations = NULL;
    int count = cupsGetDests(&destinations);
    char dest_detail[64];
    snprintf(dest_detail, sizeof(dest_detail), "count=%d", count);
    cups_dest_t *wanted = cupsGetDest("bionicx-test", NULL, count, destinations);
    check("cupsGetDests-bionicx-test", wanted != NULL, dest_detail);

    const char *uri = NULL;
    if (wanted != NULL)
        uri = cupsGetOption("device-uri", wanted->num_options, wanted->options);
    const char *output = file_uri_path(uri);
    check("file-device-uri", output != NULL && output[0] == '/',
          uri != NULL ? uri : "missing");

    const char *tmpdir = getenv("BIONICX_TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') tmpdir = "/tmp";
    char fixture[256];
    snprintf(fixture, sizeof(fixture), "%s/bionicx-cups-probe.txt", tmpdir);
    const char marker[] = "BIONICX_CUPS_PRINT_PROBE\n";
    int fd = open(fixture, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int wrote = fd >= 0 && write(fd, marker, sizeof(marker) - 1)
            == (ssize_t)(sizeof(marker) - 1);
    if (fd >= 0) close(fd);
    check("write-fixture", wrote, fixture);

    int job = 0;
    if (wanted != NULL && wrote) {
        if (output != NULL) unlink(output);
        job = cupsPrintFile("bionicx-test", fixture, "bionicx-cups-probe",
                            0, NULL);
    }
    char job_detail[80];
    snprintf(job_detail, sizeof(job_detail), "job=%d err=%s", job,
             cupsLastErrorString());
    check("cupsPrintFile", job > 0, job_detail);

    int saw_output = 0;
    char payload[64];
    payload[0] = '\0';
    if (job > 0 && output != NULL) {
        for (int i = 0; i < 80; ++i) {
            int outfd = open(output, O_RDONLY);
            ssize_t n = outfd >= 0 ? read(outfd, payload, sizeof(payload) - 1) : -1;
            if (outfd >= 0) close(outfd);
            if (n > 0) {
                payload[n] = '\0';
                if (strstr(payload, "BIONICX_CUPS_PRINT_PROBE") != NULL) {
                    saw_output = 1;
                    break;
                }
            }
            usleep(100000);
        }
    }
    if (!saw_output && job > 0) {
        /* The file: backend may not copy before this client exits. The
           submitted payload is still in the CUPS request spool. */
        const char *spool = getenv("CUPS_SERVERROOT");
        if (spool != NULL) {
            char newest[256];
            snprintf(newest, sizeof(newest), "%s/spool/d%05d-001", spool, job);
            int outfd = open(newest, O_RDONLY);
            ssize_t n = outfd >= 0 ? read(outfd, payload, sizeof(payload) - 1) : -1;
            if (outfd >= 0) close(outfd);
            if (n > 0) {
                payload[n] = '\0';
                if (strstr(payload, "BIONICX_CUPS_PRINT_PROBE") != NULL)
                    saw_output = 1;
            }
        }
    }
    check("job-payload", saw_output, payload[0] ? payload : "missing");

    cupsFreeDests(count, destinations);
    dlclose(handle);
    printf("BXSUMMARY wps-cups passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
