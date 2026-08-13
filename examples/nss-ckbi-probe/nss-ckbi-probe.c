#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <link.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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

typedef int (*nss_init_fn)(const char *);
typedef int (*nss_shutdown_fn)(void);
typedef void *(*pk11_list_certs_fn)(int, void *);
typedef void (*cert_destroy_list_fn)(void *);

int main(void) {
    const char *gred = getenv("MOZILLA_FIVE_HOME");
    if (gred == NULL || gred[0] == '\0')
        gred = "/usr/lib/firefox-esr";

    char gred_softokn[512];
    char gred_ckbi[512];
    snprintf(gred_softokn, sizeof(gred_softokn), "%s/libsoftokn3.so", gred);
    snprintf(gred_ckbi, sizeof(gred_ckbi), "%s/libnssckbi.so", gred);

    check("gred-softokn", access(gred_softokn, R_OK) == 0, gred_softokn);
    check("gred-ckbi", access(gred_ckbi, R_OK) == 0, gred_ckbi);

    const char *root = getenv("BIONICX_ROOTFS");
    char system_ckbi[512] = "/usr/lib/aarch64-linux-gnu/libnssckbi.so";
    if (root != NULL && root[0] == '/') {
        snprintf(system_ckbi, sizeof(system_ckbi),
                 "%s/usr/lib/aarch64-linux-gnu/libnssckbi.so", root);
    }
    check("system-ckbi", access(system_ckbi, R_OK) == 0, system_ckbi);

    if (access(gred_ckbi, R_OK) == 0 && access(system_ckbi, R_OK) == 0) {
        char target[512];
        ssize_t n = readlink(gred_ckbi, target, sizeof(target) - 1);
        if (n > 0) {
            target[n] = '\0';
            check("gred-ckbi-relative",
                  target[0] != '/' && strstr(target, "libnssckbi.so") != NULL,
                  target);
        } else {
            check("gred-ckbi-relative", 1, "regular-file");
        }
    }

    /* Firefox loads NSS from GreD, not the multiarch SONAME search path. */
    char gred_nss3[512];
    snprintf(gred_nss3, sizeof(gred_nss3), "%s/libnss3.so", gred);
    void *nss = dlopen(gred_nss3, RTLD_NOW);
    check("dlopen-libnss3", nss != NULL, nss ? gred_nss3 : dlerror());
    void *softokn = dlopen("libsoftokn3.so", RTLD_NOW);
    const char *softokn_path = NULL;
    if (softokn != NULL) {
        struct link_map *map = NULL;
        if (dlinfo(softokn, RTLD_DI_LINKMAP, &map) == 0 && map != NULL)
            softokn_path = map->l_name;
    }
    check("gred-softokn-soname",
          softokn_path != NULL && strstr(softokn_path, "firefox-esr") != NULL,
          softokn_path != NULL ? softokn_path : "unresolved");
    if (nss == NULL) {
        printf("BXSUMMARY nss-ckbi passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    nss_init_fn NSS_NoDB_Init = (nss_init_fn)dlsym(nss, "NSS_NoDB_Init");
    int (*NSS_Initialize)(const char *, const char *, const char *,
                          const char *, unsigned int) =
            dlsym(nss, "NSS_Initialize");
    int (*PORT_GetError)(void) = dlsym(nss, "PORT_GetError");
    nss_shutdown_fn NSS_Shutdown = (nss_shutdown_fn)dlsym(nss, "NSS_Shutdown");
    pk11_list_certs_fn PK11_ListCerts =
            (pk11_list_certs_fn)dlsym(nss, "PK11_ListCerts");
    cert_destroy_list_fn CERT_DestroyCertList =
            (cert_destroy_list_fn)dlsym(nss, "CERT_DestroyCertList");
    void *(*SECMOD_LoadUserModule)(char *, void *, int) =
            dlsym(nss, "SECMOD_LoadUserModule");

    check("nss-symbols",
          NSS_NoDB_Init != NULL && NSS_Shutdown != NULL
                  && PK11_ListCerts != NULL && CERT_DestroyCertList != NULL
                  && SECMOD_LoadUserModule != NULL,
          NSS_NoDB_Init ? "NSS_NoDB_Init" : "missing");

    const char *tmpdir = getenv("BIONICX_TMPDIR");
    if (tmpdir == NULL) tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) tmpdir = "/tmp";
    char sqldir[512];
    snprintf(sqldir, sizeof(sqldir), "%s/nss-ckbi-sql", tmpdir);
    if (mkdir(sqldir, 0700) != 0 && errno != EEXIST) {
        check("nss-sql-dir", 0, strerror(errno));
    }
    char sqlspec[530];
    snprintf(sqlspec, sizeof(sqlspec), "sql:%s", sqldir);
    int sql_ok = 0;
    void *(*PK11_GetInternalKeySlot)(void) =
            dlsym(nss, "PK11_GetInternalKeySlot");
    int (*PK11_NeedUserInit)(void *) = dlsym(nss, "PK11_NeedUserInit");
    int (*PK11_InitPin)(void *, const char *, const char *) =
            dlsym(nss, "PK11_InitPin");
    void (*PK11_FreeSlot)(void *) = dlsym(nss, "PK11_FreeSlot");

    if (NSS_Initialize != NULL) {
        /* Firefox psm::InitializeNSS uses NOROOTINIT|OPTIMIZESPACE (0x30). */
        sql_ok = NSS_Initialize(sqlspec, "", "", "secmod.db", 0x30) == 0;
        char sql_detail[32];
        if (sql_ok) {
            snprintf(sql_detail, sizeof(sql_detail), "sql-ok");
        } else {
            snprintf(sql_detail, sizeof(sql_detail), "err=%d",
                     PORT_GetError != NULL ? PORT_GetError() : 0);
        }
        check("nss-sql-init", sql_ok, sql_detail);
        int pin_ok = 0;
        char pin_detail[32] = "skipped";
        if (sql_ok && PK11_GetInternalKeySlot != NULL &&
                PK11_InitPin != NULL && PK11_FreeSlot != NULL) {
            void *slot = PK11_GetInternalKeySlot();
            if (slot == NULL) {
                snprintf(pin_detail, sizeof(pin_detail), "no-slot err=%d",
                         PORT_GetError != NULL ? PORT_GetError() : 0);
            } else {
                int need = PK11_NeedUserInit != NULL
                        && PK11_NeedUserInit(slot);
                if (need)
                    pin_ok = PK11_InitPin(slot, NULL, NULL) == 0;
                else
                    pin_ok = 1;
                if (pin_ok)
                    snprintf(pin_detail, sizeof(pin_detail),
                             need ? "init-pin" : "ready");
                else
                    snprintf(pin_detail, sizeof(pin_detail), "pin-err=%d",
                             PORT_GetError != NULL ? PORT_GetError() : 0);
                PK11_FreeSlot(slot);
            }
        }
        check("psm-key-slot", pin_ok, pin_detail);
        if (sql_ok && NSS_Shutdown != NULL) NSS_Shutdown();
    } else {
        check("nss-sql-init", 0, "NSS_Initialize missing");
    }

    int init_ok = NSS_NoDB_Init != NULL && NSS_NoDB_Init(NULL) == 0;
    check("nss-init", init_ok, init_ok ? "nodb" : "NSS_NoDB_Init");

    if (init_ok && SECMOD_LoadUserModule != NULL) {
        char spec[768];
        snprintf(spec, sizeof(spec),
                 "name=\"Builtin Roots\" parameters=\"\" library=\"%s\" "
                 "NSS=\"trustOrder=100\"",
                 gred_ckbi);
        void *module = SECMOD_LoadUserModule(spec, NULL, 0);
        check("load-ckbi", module != NULL, gred_ckbi);
    }

    int anchors = 0;
    if (init_ok && PK11_ListCerts != NULL && CERT_DestroyCertList != NULL) {
        /* PK11CertListUnique == 3 in nss/lib/pk11wrap/pk11pub.h */
        void *list = PK11_ListCerts(3, NULL);
        if (list != NULL) {
            /* Walk the PRCList-compatible CERTCertList. The first two
             * pointers are next/prev; we count nodes until we return. */
            struct cert_list {
                struct cert_list *next;
                struct cert_list *prev;
            };
            struct cert_list *head = list;
            for (struct cert_list *node = head->next; node != head;
                    node = node->next) {
                ++anchors;
                if (anchors > 10000) break;
            }
            CERT_DestroyCertList(list);
        }
    }
    char anchor_detail[64];
    snprintf(anchor_detail, sizeof(anchor_detail), "count=%d", anchors);
    check("trust-anchors", anchors >= 40, anchor_detail);

    char gred_ssl3[512];
    snprintf(gred_ssl3, sizeof(gred_ssl3), "%s/libssl3.so", gred);
    void *ssl = dlopen(gred_ssl3, RTLD_NOW);
    void *nspr = dlopen("libnspr4.so", RTLD_NOW);
    check("dlopen-libssl3", ssl != NULL, ssl ? gred_ssl3 : dlerror());
    check("dlopen-libnspr4", nspr != NULL, nspr ? "libnspr4.so" : dlerror());

    if (init_ok && ssl != NULL && nspr != NULL) {
        int (*NSS_SetDomesticPolicy)(void) =
                dlsym(ssl, "NSS_SetDomesticPolicy");
        if (NSS_SetDomesticPolicy == NULL)
            NSS_SetDomesticPolicy = dlsym(nss, "NSS_SetDomesticPolicy");
        void *(*PR_ImportTCPSocket)(int) =
                dlsym(nspr, "PR_ImportTCPSocket");
        int (*PR_GetError)(void) = dlsym(nspr, "PR_GetError");
        void *(*SSL_ImportFD)(void *, void *) = dlsym(ssl, "SSL_ImportFD");
        int (*SSL_SetURL)(void *, const char *) = dlsym(ssl, "SSL_SetURL");
        int (*SSL_ResetHandshake)(void *, int) =
                dlsym(ssl, "SSL_ResetHandshake");
        int (*SSL_ForceHandshake)(void *) = dlsym(ssl, "SSL_ForceHandshake");
        int (*SSL_OptionSet)(void *, int, int) = dlsym(ssl, "SSL_OptionSet");
        char missing[160] = "";
        if (NSS_SetDomesticPolicy == NULL) strcat(missing, "domestic ");
        if (PR_ImportTCPSocket == NULL) strcat(missing, "import ");
        if (SSL_ImportFD == NULL) strcat(missing, "sslfd ");
        if (SSL_SetURL == NULL) strcat(missing, "url ");
        if (SSL_ResetHandshake == NULL) strcat(missing, "reset ");
        if (SSL_ForceHandshake == NULL) strcat(missing, "force ");
        if (SSL_OptionSet == NULL) strcat(missing, "option ");
        int symbols = missing[0] == '\0';
        check("tls-symbols", symbols, symbols ? "ssl3+nspr" : missing);

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo *resolved = NULL;
        int gai = getaddrinfo("example.com", "443", &hints, &resolved);
        char resolve_detail[80] = "";
        if (gai == 0 && resolved != NULL) {
            char ip[INET_ADDRSTRLEN] = "";
            struct sockaddr_in *in =
                    (struct sockaddr_in *)resolved->ai_addr;
            inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip));
            snprintf(resolve_detail, sizeof(resolve_detail), "example.com=%s",
                     ip);
        } else {
            snprintf(resolve_detail, sizeof(resolve_detail), "gai=%d errno=%d",
                     gai, errno);
        }
        check("dns-example", gai == 0 && resolved != NULL, resolve_detail);

        int handshake_ok = 0;
        char handshake_detail[80] = "skipped";
        if (symbols && gai == 0 && resolved != NULL) {
            int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
            int connected = fd >= 0
                    && connect(fd, resolved->ai_addr,
                               resolved->ai_addrlen) == 0;
            if (!connected) {
                snprintf(handshake_detail, sizeof(handshake_detail),
                         "connect errno=%d", errno);
                if (fd >= 0) close(fd);
            } else {
                if (NSS_SetDomesticPolicy() != 0) {
                    snprintf(handshake_detail, sizeof(handshake_detail),
                             "domestic-policy");
                    close(fd);
                } else {
                    void *layer = PR_ImportTCPSocket(fd);
                    void *ssl_fd = layer != NULL
                            ? SSL_ImportFD(NULL, layer) : NULL;
                    /* SSL_SECURITY=1, SSL_ENABLE_TLS=13 */
                    if (ssl_fd != NULL) {
                        SSL_OptionSet(ssl_fd, 1, 1);
                        SSL_OptionSet(ssl_fd, 13, 1);
                        SSL_SetURL(ssl_fd, "example.com");
                        SSL_ResetHandshake(ssl_fd, 0);
                        handshake_ok = SSL_ForceHandshake(ssl_fd) == 0;
                        snprintf(handshake_detail, sizeof(handshake_detail),
                                 handshake_ok ? "tls-ok" : "pr_error=%d",
                                 PR_GetError != NULL ? PR_GetError() : -1);
                    } else {
                        snprintf(handshake_detail, sizeof(handshake_detail),
                                 "import-fd");
                        close(fd);
                    }
                }
            }
        }
        if (resolved != NULL) freeaddrinfo(resolved);
        check("tls-example", handshake_ok, handshake_detail);
    }

    printf("BXSUMMARY nss-ckbi passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
