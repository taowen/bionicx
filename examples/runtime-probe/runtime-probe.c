#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <gnu/libc-version.h>
#include <linux/futex.h>
#include <linux/memfd.h>
#include <langinfo.h>
#include <locale.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/random.h>
#include <sys/signalfd.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int passed;
static int failed;

static void check(const char *name, bool ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail && *detail ? detail : "");
    fflush(stdout);
    if (ok) ++passed;
    else ++failed;
}

static void capability(const char *name, const char *state, int value) {
    const char *description = "none";
    if (value > 0)
        description = strcmp(state, "signal") == 0 ? strsignal(value) : strerror(value);
    printf("BXCAP %s %s value=%d (%s)\n", name, state, value, description);
    fflush(stdout);
}

static void close_if_valid(int fd) {
    if (fd >= 0) close(fd);
}

static void *thread_set_value(void *opaque) {
    *(int *)opaque = 0x5a17;
    return (void *)(uintptr_t)0x31;
}

static bool test_pthread(void) {
    pthread_t thread;
    int value = 0;
    void *result = NULL;
    return pthread_create(&thread, NULL, thread_set_value, &value) == 0 &&
           pthread_join(thread, &result) == 0 && value == 0x5a17 &&
           result == (void *)(uintptr_t)0x31;
}

static pthread_mutex_t robust_mutex;

static void *leave_robust_mutex_locked(void *unused) {
    (void)unused;
    int rc = pthread_mutex_lock(&robust_mutex);
    return (void *)(intptr_t)rc;
}

static bool test_robust_mutex(char *detail, size_t size) {
    pthread_mutexattr_t attributes;
    pthread_t owner;
    void *owner_result = NULL;
    int rc = pthread_mutexattr_init(&attributes);
    if (rc != 0) {
        snprintf(detail, size, "mutexattr-init=%d (%s)", rc, strerror(rc));
        return false;
    }
    rc = pthread_mutexattr_setrobust(&attributes, PTHREAD_MUTEX_ROBUST);
    if (rc == 0) rc = pthread_mutex_init(&robust_mutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
    if (rc == 0) rc = pthread_create(&owner, NULL, leave_robust_mutex_locked, NULL);
    if (rc == 0) rc = pthread_join(owner, &owner_result);
    if (rc == 0 && (intptr_t)owner_result != 0) rc = (int)(intptr_t)owner_result;
    if (rc == 0) rc = pthread_mutex_lock(&robust_mutex);
    bool ok = rc == EOWNERDEAD;
    snprintf(detail, size, "lock-after-owner-exit=%d (%s)", rc, strerror(rc));
    if (rc == EOWNERDEAD) {
        pthread_mutex_consistent(&robust_mutex);
        pthread_mutex_unlock(&robust_mutex);
    }
    pthread_mutex_destroy(&robust_mutex);
    return ok;
}

static bool test_robust_mutex_isolated(char *detail, size_t size) {
    pid_t child = fork();
    if (child == 0) {
        char child_detail[160];
        alarm(2);
        _exit(test_robust_mutex(child_detail, sizeof(child_detail)) ? 0 : 1);
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) != child) {
        snprintf(detail, size, "fork/wait failed errno=%d (%s)", errno, strerror(errno));
        return false;
    }
    if (WIFSIGNALED(status)) {
        snprintf(detail, size, "signal=%d%s", WTERMSIG(status),
                 WTERMSIG(status) == SIGALRM ? " owner-death timeout" : "");
        return false;
    }
    snprintf(detail, size, "isolated-exit=%d", WEXITSTATUS(status));
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool test_fork_wait(void) {
    pid_t child = fork();
    if (child == 0) _exit(37);
    int status = 0;
    return child > 0 && waitpid(child, &status, 0) == child &&
           WIFEXITED(status) && WEXITSTATUS(status) == 37;
}

static bool test_eventfd_epoll(void) {
    int event = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    int epoll = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event registration = {.events = EPOLLIN, .data.fd = event};
    uint64_t sent = 9;
    uint64_t received = 0;
    struct epoll_event ready = {0};
    bool ok = event >= 0 && epoll >= 0 &&
              epoll_ctl(epoll, EPOLL_CTL_ADD, event, &registration) == 0 &&
              write(event, &sent, sizeof(sent)) == (ssize_t)sizeof(sent) &&
              epoll_wait(epoll, &ready, 1, 1000) == 1 &&
              ready.data.fd == event &&
              read(event, &received, sizeof(received)) == (ssize_t)sizeof(received) &&
              received == sent;
    close_if_valid(event);
    close_if_valid(epoll);
    return ok;
}

static bool test_timerfd(void) {
    int timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    struct itimerspec spec = {.it_value = {.tv_nsec = 20000000}};
    struct pollfd poll_fd = {.fd = timer, .events = POLLIN};
    uint64_t expirations = 0;
    bool ok = timer >= 0 && timerfd_settime(timer, 0, &spec, NULL) == 0 &&
              poll(&poll_fd, 1, 1000) == 1 &&
              read(timer, &expirations, sizeof(expirations)) ==
                      (ssize_t)sizeof(expirations) && expirations == 1;
    close_if_valid(timer);
    return ok;
}

static bool test_signalfd(void) {
    sigset_t mask;
    sigset_t old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    int rc = pthread_sigmask(SIG_BLOCK, &mask, &old_mask);
    int signal_fd = rc == 0 ? signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK) : -1;
    struct pollfd poll_fd = {.fd = signal_fd, .events = POLLIN};
    struct signalfd_siginfo info = {0};
    bool ok = signal_fd >= 0 && kill(getpid(), SIGUSR1) == 0 &&
              poll(&poll_fd, 1, 1000) == 1 &&
              read(signal_fd, &info, sizeof(info)) == (ssize_t)sizeof(info) &&
              info.ssi_signo == SIGUSR1;
    close_if_valid(signal_fd);
    if (rc == 0) pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
    return ok;
}

static bool test_memfd_mmap(void) {
    int fd = (int)syscall(SYS_memfd_create, "bionicx-runtime-probe",
                          MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0 || ftruncate(fd, 4096) != 0) {
        close_if_valid(fd);
        return false;
    }
    char *first = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    char *second = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    bool ok = first != MAP_FAILED && second != MAP_FAILED;
    if (ok) {
        memcpy(first, "memfd-shared", 13);
        ok = memcmp(second, "memfd-shared", 13) == 0;
    }
    if (first != MAP_FAILED) munmap(first, 4096);
    if (second != MAP_FAILED) munmap(second, 4096);
    close(fd);
    return ok;
}

static bool test_mprotect(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    char *page = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    bool ok = page != MAP_FAILED;
    if (ok) {
        strcpy(page, "protected");
        ok = mprotect(page, (size_t)page_size, PROT_READ) == 0 &&
             strcmp(page, "protected") == 0;
        munmap(page, (size_t)page_size);
    }
    return ok;
}

static bool test_scm_rights(void) {
    int sockets[2] = {-1, -1};
    int source = -1;
    int received = -1;
    char byte = 'F';
    char control[CMSG_SPACE(sizeof(int))] = {0};
    char recv_control[CMSG_SPACE(sizeof(int))] = {0};
    struct iovec send_iov = {.iov_base = &byte, .iov_len = 1};
    struct msghdr send_message = {.msg_iov = &send_iov, .msg_iovlen = 1,
                                  .msg_control = control,
                                  .msg_controllen = sizeof(control)};
    bool ok = socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) == 0;
    if (ok) source = open("/dev/null", O_RDONLY | O_CLOEXEC);
    struct cmsghdr *header = CMSG_FIRSTHDR(&send_message);
    if (ok && source >= 0 && header) {
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(header), &source, sizeof(source));
        ok = sendmsg(sockets[0], &send_message, 0) == 1;
    }
    else ok = false;

    char received_byte = 0;
    struct iovec recv_iov = {.iov_base = &received_byte, .iov_len = 1};
    struct msghdr recv_message = {.msg_iov = &recv_iov, .msg_iovlen = 1,
                                  .msg_control = recv_control,
                                  .msg_controllen = sizeof(recv_control)};
    if (ok) ok = recvmsg(sockets[1], &recv_message, 0) == 1;
    header = CMSG_FIRSTHDR(&recv_message);
    if (ok && header && header->cmsg_level == SOL_SOCKET &&
        header->cmsg_type == SCM_RIGHTS) {
        memcpy(&received, CMSG_DATA(header), sizeof(received));
        ok = received_byte == byte && fcntl(received, F_GETFD) >= 0;
    }
    else ok = false;
    close_if_valid(received);
    close_if_valid(source);
    close_if_valid(sockets[0]);
    close_if_valid(sockets[1]);
    return ok;
}

static bool test_unix_socket(void) {
    const char *temporary = getenv("TMPDIR");
    if (!temporary) return false;
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    int length = snprintf(path, sizeof(path), "%s/runtime-probe-%ld.sock",
                          temporary, (long)getpid());
    if (length <= 0 || (size_t)length >= sizeof(path)) return false;
    int server = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int client = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int accepted = -1;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    memcpy(address.sun_path, path, (size_t)length + 1);
    unlink(path);
    bool ok = server >= 0 && client >= 0 &&
              bind(server, (struct sockaddr *)&address, sizeof(address)) == 0 &&
              listen(server, 1) == 0 &&
              connect(client, (struct sockaddr *)&address, sizeof(address)) == 0;
    if (ok) accepted = accept4(server, NULL, NULL, SOCK_CLOEXEC);
    char sent = 'U';
    char received = 0;
    if (ok) ok = accepted >= 0 && write(client, &sent, 1) == 1 &&
                 read(accepted, &received, 1) == 1 && received == sent;
    close_if_valid(accepted);
    close_if_valid(client);
    close_if_valid(server);
    unlink(path);
    return ok;
}

static bool test_loopback_tcp(char *detail, size_t size) {
    int server = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int client = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int accepted = -1;
    struct sockaddr_in address = {.sin_family = AF_INET,
                                  .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    socklen_t address_length = sizeof(address);
    const char *stage = "socket";
    bool ok = server >= 0 && client >= 0;
    if (ok) {
        stage = "bind";
        ok = bind(server, (struct sockaddr *)&address, sizeof(address)) == 0;
    }
    if (ok) {
        stage = "getsockname";
        ok = getsockname(server, (struct sockaddr *)&address, &address_length) == 0;
    }
    if (ok) {
        stage = "listen";
        ok = listen(server, 1) == 0;
    }
    if (ok) {
        stage = "connect";
        ok = connect(client, (struct sockaddr *)&address, sizeof(address)) == 0;
    }
    int saved_errno = ok ? 0 : errno;
    if (ok) accepted = accept4(server, NULL, NULL, SOCK_CLOEXEC);
    char sent = 'T';
    char received = 0;
    if (ok) ok = accepted >= 0 && write(client, &sent, 1) == 1 &&
                 read(accepted, &received, 1) == 1 && received == sent;
    close_if_valid(accepted);
    close_if_valid(client);
    close_if_valid(server);
    snprintf(detail, size, "%s%s%s", stage, ok ? "" : " errno=",
             ok ? "" : strerror(saved_errno));
    return ok;
}

static bool test_inotify(void) {
    const char *temporary = getenv("TMPDIR");
    int fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    int watch = fd >= 0 && temporary ? inotify_add_watch(fd, temporary, IN_CREATE) : -1;
    bool ok = fd >= 0 && watch >= 0;
    close_if_valid(fd);
    return ok;
}

static bool test_procfs(void) {
    int maps = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    int descriptors = open("/proc/self/fd", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    bool ok = maps >= 0 && descriptors >= 0;
    close_if_valid(maps);
    close_if_valid(descriptors);
    return ok;
}

static bool test_dlopen(void) {
    void *library = dlopen("libm.so.6", RTLD_NOW | RTLD_LOCAL);
    double (*cosine)(double) = library ? (double (*)(double))dlsym(library, "cos") : NULL;
    bool ok = cosine && fabs(cosine(0.0) - 1.0) < 0.000001;
    if (library) dlclose(library);
    return ok;
}

static bool test_getrandom(void) {
    unsigned char random_data[32] = {0};
    return getrandom(random_data, sizeof(random_data), 0) ==
           (ssize_t)sizeof(random_data);
}

static bool test_unnamed_semaphore(void) {
    sem_t semaphore;
    bool initialized = sem_init(&semaphore, 0, 2) == 0;
    bool ok = initialized && sem_wait(&semaphore) == 0 && sem_post(&semaphore) == 0;
    if (initialized) sem_destroy(&semaphore);
    return ok;
}

static bool test_prctl_name(void) {
    char name[17] = {0};
    return prctl(PR_SET_NAME, "bionicx-probe", 0, 0, 0) == 0 &&
           prctl(PR_GET_NAME, name, 0, 0, 0) == 0 &&
           strcmp(name, "bionicx-probe") == 0;
}

static bool test_locale(void) {
    const char *locale = setlocale(LC_ALL, "");
    const char *codeset = locale ? nl_langinfo(CODESET) : NULL;
    return locale && codeset && strcasecmp(codeset, "UTF-8") == 0;
}

static bool test_android_shell_popen(char *detail, size_t size) {
    FILE *stream = popen("printf bionicx-android-shell", "r");
    char output[64] = {0};
    size_t length = stream ? fread(output, 1, sizeof(output) - 1, stream) : 0;
    int status = stream ? pclose(stream) : -1;
    bool ok = length == strlen("bionicx-android-shell")
              && strcmp(output, "bionicx-android-shell") == 0
              && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    snprintf(detail, size, "bytes=%zu status=%d output=%s", length, status,
             length ? output : "<empty>");
    return ok;
}

typedef int (*isolated_test)(void);

static void run_capability(const char *name, isolated_test test) {
    pid_t child = fork();
    if (child == 0) {
        errno = 0;
        int rc = test();
        _exit(rc == 0 ? 0 : (errno > 0 && errno < 126 ? errno : 125));
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) != child) {
        capability(name, "fork-failed", errno);
    }
    else if (WIFSIGNALED(status)) {
        capability(name, "signal", WTERMSIG(status));
    }
    else if (WEXITSTATUS(status) == 0) {
        capability(name, "available", 0);
    }
    else {
        capability(name, "denied", WEXITSTATUS(status));
    }
}

static int capability_set_robust_list(void) {
    struct robust_list_head head = {0};
    return (int)syscall(SYS_set_robust_list, &head, sizeof(head));
}

static int capability_user_namespace(void) {
    return unshare(CLONE_NEWUSER);
}

static int capability_sysv_shm(void) {
    int id = (int)syscall(SYS_shmget, IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    if (id < 0) return -1;
    syscall(SYS_shmctl, id, IPC_RMID, NULL);
    return 0;
}

static int capability_posix_shm(void) {
    char name[64];
    snprintf(name, sizeof(name), "/bionicx-runtime-%ld", (long)getpid());
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd >= 0) {
        shm_unlink(name);
        close(fd);
        return 0;
    }
    return -1;
}

static bool show_x11_result(int duration) {
    Display *display = XOpenDisplay(NULL);
    if (!display) return false;
    int screen = DefaultScreen(display);
    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                         0, 0, (unsigned)width, (unsigned)height,
                                         0, 0, failed ? 0x3b1720 : 0x122b1d);
    XStoreName(display, window, "BionicX glibc/kernel runtime probe");
    XClassHint class_hint = {.res_name = "bionicx-runtime-probe",
                             .res_class = "BionicXRuntimeProbe"};
    XSetClassHint(display, window, &class_hint);
    XWMHints hints = {.flags = WindowGroupHint, .window_group = window};
    XSetWMHints(display, window, &hints);
    XMapWindow(display, window);
    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, 0xffffff);
    char summary[128];
    snprintf(summary, sizeof(summary),
             "BionicX runtime probe: %d passed, %d failed; see BXTEST/BXCAP", passed, failed);
    XDrawString(display, window, gc, 40, 64, summary, (int)strlen(summary));
    XSync(display, False);
    sleep((unsigned)duration);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return true;
}

int main(int argc, char **argv) {
    int duration = 8;
    if (argc == 3 && strcmp(argv[1], "--duration") == 0) duration = atoi(argv[2]);
    if (duration < 1 || duration > 300) return 2;

    const char *expected_argv0 = getenv("BIONICX_EXPECT_ARGV0");
    if (expected_argv0 != NULL) {
        char argv0_detail[256];
        snprintf(argv0_detail, sizeof(argv0_detail), "expected=%s actual=%s",
                 expected_argv0, argv[0]);
        check("loader-argv0", strcmp(argv[0], expected_argv0) == 0,
              argv0_detail);
    }

    char detail[160];
    snprintf(detail, sizeof(detail), "glibc=%s pid=%ld page=%ld",
             gnu_get_libc_version(), (long)getpid(), sysconf(_SC_PAGESIZE));
    check("runtime-identity", true, detail);
    check("pthread-create-join", test_pthread(), "");
    check("pthread-robust-mutex", test_robust_mutex_isolated(detail, sizeof(detail)), detail);
    check("fork-wait", test_fork_wait(), "");
    check("eventfd-epoll", test_eventfd_epoll(), "");
    check("timerfd-poll", test_timerfd(), "");
    check("signalfd", test_signalfd(), "");
    check("memfd-shared-mmap", test_memfd_mmap(), "");
    check("mprotect", test_mprotect(), "");
    check("unix-scm-rights", test_scm_rights(), "");
    check("unix-filesystem-socket", test_unix_socket(), "");
    check("tcp-loopback", test_loopback_tcp(detail, sizeof(detail)), detail);
    check("inotify", test_inotify(), "");
    check("procfs", test_procfs(), "");
    check("dlopen-libm", test_dlopen(), "");
    check("getrandom", test_getrandom(), "");
    check("unnamed-semaphore", test_unnamed_semaphore(), "");
    check("locale-utf8", test_locale(), "");
    check("prctl-name", test_prctl_name(), "");
    if (getenv("BIONICX_TEST_ANDROID_SHELL") != NULL) {
        check("android-shell-popen",
              test_android_shell_popen(detail, sizeof(detail)), detail);
    }

    run_capability("raw-set-robust-list", capability_set_robust_list);
    run_capability("user-namespace", capability_user_namespace);
    run_capability("sysv-shm", capability_sysv_shm);
    run_capability("posix-shm", capability_posix_shm);

    bool x11 = show_x11_result(duration);
    check("x11-window-text", x11, "");
    printf("BXSUMMARY runtime passed=%d failed=%d\n", passed, failed);
    fflush(stdout);
    return failed == 0 ? 0 : 1;
}
