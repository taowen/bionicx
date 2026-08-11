#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int passed;
static int failed;

static void check(const char *name, bool ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail != NULL && detail[0] != '\0' ? " " : "",
           detail != NULL ? detail : "");
    fflush(stdout);
    if (ok) ++passed;
    else ++failed;
}

static bool first_configured_server(res_state state, char *detail, size_t size) {
    char address[INET_ADDRSTRLEN] = "";
    bool ok = state->nscount > 0 &&
              state->nsaddr_list[0].sin_family == AF_INET &&
              state->nsaddr_list[0].sin_port == htons(NS_DEFAULTPORT) &&
              inet_ntop(AF_INET, &state->nsaddr_list[0].sin_addr,
                        address, sizeof(address)) != NULL;
    snprintf(detail, size, "nscount=%d first=%s", state->nscount,
             address[0] != '\0' ? address : "none");
    return ok;
}

static bool resolve_a(res_state state, const char *host,
                      struct in_addr *address, char *detail, size_t size) {
    unsigned char answer[NS_PACKETSZ * 4];
    int length = res_nquery(state, host, ns_c_in, ns_t_a,
                            answer, sizeof(answer));
    if (length < 0) {
        snprintf(detail, size, "h_errno=%d", h_errno);
        return false;
    }

    ns_msg message;
    if (ns_initparse(answer, length, &message) != 0) {
        snprintf(detail, size, "ns_initparse errno=%d", errno);
        return false;
    }
    int answers = ns_msg_count(message, ns_s_an);
    for (int index = 0; index < answers; ++index) {
        ns_rr record;
        if (ns_parserr(&message, ns_s_an, index, &record) == 0 &&
                ns_rr_type(record) == ns_t_a && ns_rr_rdlen(record) == 4) {
            memcpy(address, ns_rr_rdata(record), sizeof(*address));
            char rendered[INET_ADDRSTRLEN] = "";
            inet_ntop(AF_INET, address, rendered, sizeof(rendered));
            snprintf(detail, size, "answers=%d address=%s", answers, rendered);
            return true;
        }
    }
    snprintf(detail, size, "answers=%d no-A-record", answers);
    return false;
}

static bool fetch_http(const char *host, struct in_addr address,
                       char *detail, size_t size) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in destination = {
        .sin_family = AF_INET,
        .sin_port = htons(80),
        .sin_addr = address,
    };
    struct timeval timeout = {.tv_sec = 5};
    bool ok = fd >= 0 &&
              setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
              setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0 &&
              connect(fd, (struct sockaddr *)&destination, sizeof(destination)) == 0;
    const char request_format[] =
        "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n"
        "User-Agent: bionicx-network-probe/1\r\n\r\n";
    char request[256];
    int request_length = snprintf(request, sizeof(request), request_format, host);
    if (ok) ok = request_length > 0 && request_length < (int)sizeof(request) &&
                 send(fd, request, (size_t)request_length, MSG_NOSIGNAL) == request_length;

    char response[8192];
    size_t used = 0;
    while (ok && used + 1 < sizeof(response)) {
        ssize_t count = recv(fd, response + used, sizeof(response) - used - 1, 0);
        if (count == 0) break;
        if (count < 0) {
            ok = false;
            break;
        }
        used += (size_t)count;
    }
    if (fd >= 0) close(fd);
    response[used] = '\0';
    ok = ok && used > 0 && strstr(response, "HTTP/1.1 200") != NULL &&
         strstr(response, "Example Domain") != NULL;
    snprintf(detail, size, "bytes=%zu status=%s body=%s", used,
             strstr(response, "HTTP/1.1 200") != NULL ? "200" : "other",
             strstr(response, "Example Domain") != NULL ? "matched" : "missing");
    return ok;
}

static bool show_x11_result(int duration, char *detail, size_t size) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        snprintf(detail, size, "display-open-failed");
        return false;
    }
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        80, 80, 720, 180, 1,
                                        BlackPixel(display, screen),
                                        WhitePixel(display, screen));
    XStoreName(display, window, "BionicX network probe");
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);
    XFlush(display);

    time_t deadline = time(NULL) + duration;
    bool exposed = false;
    while (time(NULL) < deadline) {
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == Expose) {
                const char message[] = "DNS + TCP + HTTP passed from a real glibc client";
                XDrawString(display, window, DefaultGC(display, screen),
                            32, 92, message, (int)strlen(message));
                exposed = true;
            }
        }
        usleep(10000);
    }
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    snprintf(detail, size, "expose=%s", exposed ? "yes" : "no");
    return exposed;
}

int main(int argc, char **argv) {
    const char *host = "example.com";
    int duration = 3;
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--host") == 0 && index + 1 < argc)
            host = argv[++index];
        else if (strcmp(argv[index], "--duration") == 0 && index + 1 < argc)
            duration = atoi(argv[++index]);
    }

    struct __res_state resolver;
    memset(&resolver, 0, sizeof(resolver));
    int initialized = res_ninit(&resolver);
    check("resolver-init", initialized == 0,
          initialized == 0 ? "glibc-res_state-ready" : "res_ninit-failed");

    char detail[192];
    bool configured = initialized == 0 &&
                      first_configured_server(&resolver, detail, sizeof(detail));
    check("android-dns-config", configured, detail);

    struct in_addr address = {0};
    bool resolved = configured && resolve_a(&resolver, host, &address,
                                             detail, sizeof(detail));
    check("dns-a-query", resolved, detail);

    bool fetched = resolved && fetch_http(host, address, detail, sizeof(detail));
    check("tcp-http", fetched, detail);

    bool shown = fetched && show_x11_result(duration, detail, sizeof(detail));
    check("x11-result-window", shown, detail);
    if (initialized == 0) res_nclose(&resolver);

    printf("BXTEST SUMMARY pass=%d fail=%d\n", passed, failed);
    fflush(stdout);
    return failed == 0 ? 0 : 1;
}
