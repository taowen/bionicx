#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int virtual_root(void) {
    const char *value = getenv("BIONICX_VIRTUAL_ROOT");
    return value != NULL && strcmp(value, "1") == 0;
}

uid_t getuid(void) {
    static uid_t (*next)(void);
    if (next == NULL) next = dlsym(RTLD_NEXT, "getuid");
    return virtual_root() ? 0 : next();
}

uid_t geteuid(void) {
    static uid_t (*next)(void);
    if (next == NULL) next = dlsym(RTLD_NEXT, "geteuid");
    return virtual_root() ? 0 : next();
}

gid_t getgid(void) {
    static gid_t (*next)(void);
    if (next == NULL) next = dlsym(RTLD_NEXT, "getgid");
    return virtual_root() ? 0 : next();
}

gid_t getegid(void) {
    static gid_t (*next)(void);
    if (next == NULL) next = dlsym(RTLD_NEXT, "getegid");
    return virtual_root() ? 0 : next();
}

static const char *synthetic_shell(void) {
    const char *shell = getenv("SHELL");
    /* Android's app environment often has SHELL=/system/bin/sh. Electron
     * reads userInfo().shell and runs `$SHELL -ilc`; that Bionic path is
     * rewritten into a missing rootfs file. Guest profiles may set SHELL
     * to /bin/bash instead. */
    if (shell == NULL || shell[0] != '/' ||
            strncmp(shell, "/system/", 8) == 0)
        return "/bin/sh";
    return shell;
}

static int synthetic_user(uid_t uid, struct passwd *value, char *buffer,
                          size_t length, struct passwd **result) {
    const char *home = getenv("HOME");
    if (home == NULL || home[0] != '/') home = "/tmp";
    const char *fields[] = {"bionicx", "x", "BionicX Android app", home,
                            synthetic_shell()};
    size_t required = 0;
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        required += strlen(fields[i]) + 1;
    if (length < required) return ERANGE;
    char *cursor = buffer;
#define COPY_FIELD(member, index) do { \
    value->member = cursor; \
    size_t field_length = strlen(fields[index]) + 1; \
    memcpy(cursor, fields[index], field_length); \
    cursor += field_length; \
} while (0)
    COPY_FIELD(pw_name, 0);
    COPY_FIELD(pw_passwd, 1);
    COPY_FIELD(pw_gecos, 2);
    COPY_FIELD(pw_dir, 3);
    COPY_FIELD(pw_shell, 4);
#undef COPY_FIELD
    value->pw_uid = uid;
    value->pw_gid = getegid();
    *result = value;
    return 0;
}

int getpwuid_r(uid_t uid, struct passwd *value, char *buffer, size_t length,
               struct passwd **result) {
    static int (*next)(uid_t, struct passwd *, char *, size_t,
                       struct passwd **);
    if (next == NULL) next = dlsym(RTLD_NEXT, "getpwuid_r");
    if (uid == geteuid() && getenv("BIONICX_ROOTFS") != NULL)
        return synthetic_user(uid, value, buffer, length, result);
    int status = next(uid, value, buffer, length, result);
    if ((status != 0 || *result == NULL) && uid == geteuid())
        return synthetic_user(uid, value, buffer, length, result);
    return status;
}

struct passwd *getpwuid(uid_t uid) {
    static __thread struct passwd value;
    static __thread char buffer[PATH_MAX];
    struct passwd *result = NULL;
    int status = getpwuid_r(uid, &value, buffer, sizeof(buffer), &result);
    if (status != 0) errno = status;
    return status == 0 ? result : NULL;
}

static int synthetic_group(gid_t gid, struct group *value, char *buffer,
                           size_t length, struct group **result) {
    static __thread char *members[] = {NULL};
    const char name[] = "bionicx";
    const char password[] = "x";
    if (length < sizeof(name) + sizeof(password)) return ERANGE;
    memcpy(buffer, name, sizeof(name));
    memcpy(buffer + sizeof(name), password, sizeof(password));
    value->gr_name = buffer;
    value->gr_passwd = buffer + sizeof(name);
    value->gr_gid = gid;
    value->gr_mem = members;
    *result = value;
    return 0;
}

int getgrgid_r(gid_t gid, struct group *value, char *buffer, size_t length,
               struct group **result) {
    static int (*next)(gid_t, struct group *, char *, size_t, struct group **);
    if (next == NULL) next = dlsym(RTLD_NEXT, "getgrgid_r");
    int status = next(gid, value, buffer, length, result);
    if ((status != 0 || *result == NULL) && gid == getegid())
        return synthetic_group(gid, value, buffer, length, result);
    return status;
}

struct group *getgrgid(gid_t gid) {
    static __thread struct group value;
    static __thread char buffer[PATH_MAX];
    struct group *result = NULL;
    int status = getgrgid_r(gid, &value, buffer, sizeof(buffer), &result);
    if (status != 0) errno = status;
    return status == 0 ? result : NULL;
}

/* Debian groupadd aborts when libaudit cannot open the netlink socket.
 * Android app UIDs never get that socket. A /dev/null fd plus successful
 * log stubs let maintainer scripts create lpadmin/ssl-cert. */
int audit_open(void) {
    static int (*real_open)(const char *, int, ...);
    if (real_open == NULL) real_open = dlsym(RTLD_NEXT, "open");
    int fd = real_open("/dev/null", O_RDWR | O_CLOEXEC);
    if (fd >= 0) return fd;
    int ends[2];
    if (pipe(ends) != 0) return -1;
    close(ends[0]);
    return ends[1];
}

int audit_close(int fd) {
    return close(fd);
}

int audit_log_acct_message(int audit_fd, int type, const char *pgname,
                           const char *op, const char *name, unsigned int id,
                           const char *host, const char *addr, const char *tty,
                           int result) {
    (void)audit_fd;
    (void)type;
    (void)pgname;
    (void)op;
    (void)name;
    (void)id;
    (void)host;
    (void)addr;
    (void)tty;
    (void)result;
    return 1;
}

int audit_log_user_message(int audit_fd, int type, const char *message,
                           const char *hostname, const char *addr,
                           const char *tty, int result) {
    (void)audit_fd;
    (void)type;
    (void)message;
    (void)hostname;
    (void)addr;
    (void)tty;
    (void)result;
    return 1;
}

int audit_log_user_comm_message(int audit_fd, int type, const char *message,
                                const char *comm, const char *hostname,
                                const char *addr, const char *tty, int result) {
    (void)audit_fd;
    (void)type;
    (void)message;
    (void)comm;
    (void)hostname;
    (void)addr;
    (void)tty;
    (void)result;
    return 1;
}
