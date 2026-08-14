#include <X11/Xlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct query_state {
    Display *display;
    atomic_bool started;
    atomic_bool completed;
    int revert_to;
    Window focus;
};

struct open_state {
    atomic_bool completed;
    Display *display;
};

static double now_seconds(void) {
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return value.tv_sec + value.tv_nsec / 1000000000.0;
}

static void *query_focus(void *opaque) {
    struct query_state *state = opaque;
    atomic_store(&state->started, true);
    XGetInputFocus(state->display, &state->focus, &state->revert_to);
    atomic_store(&state->completed, true);
    return NULL;
}

static void *open_connection(void *opaque) {
    struct open_state *state = opaque;
    state->display = XOpenDisplay(NULL);
    atomic_store(&state->completed, true);
    return NULL;
}

static bool wait_flag(atomic_bool *flag, double timeout) {
    double deadline = now_seconds() + timeout;
    while (!atomic_load(flag) && now_seconds() < deadline) usleep(1000);
    return atomic_load(flag);
}

static void start_query(pthread_t *thread, struct query_state *state,
                        Display *display) {
    *state = (struct query_state){.display = display};
    atomic_init(&state->started, false);
    atomic_init(&state->completed, false);
    if (pthread_create(thread, NULL, query_focus, state) != 0) {
        fprintf(stderr, "BXFAIL pthread_create\n");
        exit(2);
    }
    if (!wait_flag(&state->started, 1.0)) {
        fprintf(stderr, "BXFAIL worker did not start\n");
        exit(2);
    }
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 4;
    if (!XInitThreads()) {
        fprintf(stderr, "BXFAIL XInitThreads\n");
        return 2;
    }
    Display *owner = XOpenDisplay(NULL);
    Display *peer = XOpenDisplay(NULL);
    if (!owner || !peer) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }

    XGrabServer(owner);
    XSync(owner, False);
    pthread_t worker;
    struct query_state state;
    start_query(&worker, &state, peer);
    usleep(250000);
    bool peer_frozen = !atomic_load(&state.completed);

    Atom owner_atom = XInternAtom(owner, "BIONICX_SERVER_GRAB_OWNER", False);
    XSync(owner, False);
    bool owner_progress = owner_atom != None;

    Display *new_connection = NULL;
    pthread_t connection_worker;

    XUngrabServer(owner);
    XSync(owner, False);
    bool peer_resumed = wait_flag(&state.completed, 2.0);
    pthread_join(worker, NULL);

    XGrabServer(owner);
    XSync(owner, False);
    /* XOpenDisplay performs the setup handshake and must wait behind the grab. */
    struct open_state open_state;
    atomic_init(&open_state.completed, false);
    open_state.display = NULL;
    pthread_create(&connection_worker, NULL, open_connection, &open_state);
    usleep(250000);
    bool new_connection_frozen = !atomic_load(&open_state.completed);
    XUngrabServer(owner);
    XSync(owner, False);
    bool new_connection_resumed = wait_flag(&open_state.completed, 2.0);
    pthread_join(connection_worker, NULL);
    new_connection = open_state.display;

    Display *transient = XOpenDisplay(NULL);
    bool disconnect_release = transient != NULL;
    if (transient) {
        XGrabServer(transient);
        XSync(transient, False);
        start_query(&worker, &state, peer);
        usleep(250000);
        disconnect_release = !atomic_load(&state.completed);
        XCloseDisplay(transient);
        disconnect_release = disconnect_release
                && wait_flag(&state.completed, 2.0);
        pthread_join(worker, NULL);
    }

    printf("BXTEST %s server-grab-peer-frozen exact=%d\n",
            peer_frozen ? "PASS" : "FAIL", peer_frozen);
    printf("BXTEST %s server-grab-owner-progress atom=%lu\n",
            owner_progress ? "PASS" : "FAIL", owner_atom);
    printf("BXTEST %s server-ungrab-peer-resumed exact=%d\n",
            peer_resumed ? "PASS" : "FAIL", peer_resumed);
    printf("BXTEST %s server-grab-disconnect-release exact=%d\n",
            disconnect_release ? "PASS" : "FAIL", disconnect_release);
    printf("BXTEST %s server-grab-new-connection freeze=%d resume=%d\n",
            new_connection_frozen && new_connection_resumed && new_connection
                    ? "PASS" : "FAIL",
            new_connection_frozen, new_connection_resumed);

    bool grab_map = false;
    Display *manager = XOpenDisplay(NULL);
    if (manager) {
        Window root = DefaultRootWindow(manager);
        XSelectInput(manager, root, SubstructureRedirectMask);
        XSync(manager, False);
        XGrabServer(owner);
        XSync(owner, False);
        Window mapped = XCreateSimpleWindow(owner, DefaultRootWindow(owner),
                20, 20, 120, 80, 0, 0, 0x224466);
        XMapWindow(owner, mapped);
        XSync(owner, False);
        XWindowAttributes attributes = {0};
        grab_map = XGetWindowAttributes(owner, mapped, &attributes)
                && attributes.map_state == IsViewable;
        XUngrabServer(owner);
        XDestroyWindow(owner, mapped);
        XSync(owner, False);
        XCloseDisplay(manager);
    }
    printf("BXTEST %s server-grab-map-viewable map=%d\n",
            grab_map ? "PASS" : "FAIL", grab_map);

    bool passed = peer_frozen && owner_progress && peer_resumed
            && disconnect_release && new_connection_frozen
            && new_connection_resumed && new_connection && grab_map;
    printf("BXSUMMARY server-grab-x11 passed=%d/6\n", passed ? 6 : 0);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 60 ? duration : 4));

    if (new_connection) XCloseDisplay(new_connection);
    XCloseDisplay(peer);
    XCloseDisplay(owner);
    return passed ? 0 : 1;
}
