#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dlfcn.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>

typedef int (*res_ninit_function)(res_state state);

static res_ninit_function real_res_ninit;

__attribute__((constructor)) static void initialize_android_dns(void) {
    real_res_ninit = (res_ninit_function)dlsym(RTLD_NEXT, "__res_ninit");
}

/*
 * Android publishes per-network DNS through ConnectivityManager/netd and does
 * not maintain glibc's /etc/resolv.conf.  Preserve all other resolver defaults
 * from glibc, then replace its IPv4 nameserver list with the host-provided one.
 */
int __res_ninit(res_state state) {
    int result = real_res_ninit != NULL ? real_res_ninit(state) : -1;
    const char *configured = getenv("BIONICX_DNS_SERVERS");
    if (configured == NULL || configured[0] == '\0') return result;

    char servers[256];
    size_t length = strlen(configured);
    if (length >= sizeof(servers)) return result;
    memcpy(servers, configured, length + 1);

    int count = 0;
    char *save = NULL;
    for (char *item = strtok_r(servers, ",", &save);
         item != NULL && count < MAXNS;
         item = strtok_r(NULL, ",", &save)) {
        struct in_addr address;
        if (inet_pton(AF_INET, item, &address) != 1) continue;
        struct sockaddr_in *server = &state->nsaddr_list[count];
        memset(server, 0, sizeof(*server));
        server->sin_family = AF_INET;
        server->sin_port = htons(NS_DEFAULTPORT);
        server->sin_addr = address;
        count++;
    }
    if (count == 0) return result;

    for (int index = count; index < MAXNS; index++)
        memset(&state->nsaddr_list[index], 0, sizeof(state->nsaddr_list[index]));
    state->nscount = count;
    state->options |= RES_INIT;
    return 0;
}
