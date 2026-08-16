#define _GNU_SOURCE
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks;
static int passed;
static int x_errors;
static int xkb_event_type = -1;

static int handle_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR code=%u request=%u minor=%u resource=0x%lx %s\n",
            event->error_code, event->request_code, event->minor_code,
            event->resourceid, text);
    x_errors++;
    return 0;
}

static void result(const char *name, bool ok, const char *detail) {
    checks++;
    if (ok) passed++;
    printf("BXTEST %s %-12s %s\n", ok ? "PASS" : "FAIL", name, detail);
    fflush(stdout);
}

static bool sync_without_error(Display *display, int before) {
    XSync(display, False);
    return x_errors == before;
}

static void probe_xkb(Display *display) {
    int opcode = 0, event_base = 0, error_base = 0;
    int major = XkbMajorVersion, minor = XkbMinorVersion;
    if (!XkbQueryExtension(display, &opcode, &event_base, &error_base,
                           &major, &minor)) {
        result("xkeyboard", false, "extension-missing");
        return;
    }
    xkb_event_type = event_base;
    int before = x_errors;
    bool selected = XkbSelectEvents(display, XkbUseCoreKbd,
                                    XkbStateNotifyMask,
                                    XkbStateNotifyMask);
    XkbDescPtr map = XkbGetMap(display, XkbAllClientInfoMask,
                               XkbUseCoreKbd);
    int names_status = map ? XkbGetNames(
        display, XkbComponentNamesMask | XkbKeyTypeNamesMask
                 | XkbKTLevelNamesMask | XkbKeyNamesMask
                 | XkbVirtualModNamesMask,
        map) : BadImplementation;
    XkbDeviceInfoPtr device = XkbGetDeviceInfo(
        display, XkbXI_AllDeviceFeaturesMask, XkbUseCoreKbd,
        XkbDfltXIClass, XkbDfltXIId);
    XkbDescPtr named = XkbGetKeyboard(
        display, XkbGBN_TypesMask | XkbGBN_ClientSymbolsMask,
        XkbUseCoreKbd);
    int map_status = map ? Success : BadImplementation;
    int key_syms = map && map->map && map->map->key_sym_map
                   ? XkbKeyNumSyms(map, 9) : 0;
    int sym_offset = map && map->map && map->map->key_sym_map
                     ? map->map->key_sym_map[9].offset : 0;
    KeySym escape = map && map->map && map->map->key_sym_map
                    ? XkbKeySymEntry(map, 9, 0, 0) : NoSymbol;
    KeySym lower_a = map && map->map && map->map->key_sym_map
                     ? XkbKeySymEntry(map, 38, 0, 0) : NoSymbol;
    KeySym upper_a = map && map->map && map->map->key_sym_map
                     ? XkbKeySymEntry(map, 38, 1, 0) : NoSymbol;
    bool named_map_ok = named && named->map
                        && XkbKeyNumSyms(named, 38) == 2
                        && XkbKeySymEntry(named, 38, 0, 0) == XK_a
                        && XkbKeySymEntry(named, 38, 1, 0) == XK_A;
    bool ok = selected && device && device->name
              && strcmp(device->name, "BionicX keyboard") == 0
              && device->device_spec == 3 && map_status == Success
              && map && map->map && names_status == Success && map->names
              && map->names->symbols != None && map->map->types[0].name != None
              && map->map->types[1].level_names
              && map->map->types[1].level_names[1] != None
              && memcmp(map->names->keys[9].name, "ESC\0", 4) == 0
              && map->map->num_types >= 1
              && map->min_key_code <= 9 && map->max_key_code >= 9
              && XkbKeyNumSyms(map, 9) == 1
              && escape == XK_Escape
              && XkbKeyNumSyms(map, 38) == 2
              && lower_a == XK_a && upper_a == XK_A
              && named_map_ok
              && sync_without_error(display, before);
    char detail[208];
    snprintf(detail, sizeof(detail),
             "version=%d.%d opcode=%d selected=%d device=%s map=%d names=%d keys=%u-%u types=%d esc-syms=%d offset=%d escape=0x%lx a-syms=%d a=0x%lx/0x%lx",
             major, minor, opcode, selected,
             device && device->name ? device->name : "(null)", map_status,
             names_status,
             map ? map->min_key_code : 0,
             map ? map->max_key_code : 0,
             map && map->map ? map->map->num_types : 0,
             key_syms, sym_offset, (unsigned long)escape,
             map && map->map ? XkbKeyNumSyms(map, 38) : 0,
             (unsigned long)lower_a, (unsigned long)upper_a);
    if (map) XkbFreeKeyboard(map, XkbAllComponentsMask, True);
    if (device) XkbFreeDeviceInfo(device, XkbXI_AllDeviceFeaturesMask, True);
    if (named) XkbFreeKeyboard(named, XkbAllComponentsMask, True);
    result("xkeyboard", ok, detail);
}

static void probe_xkbcommon(Display *display) {
    xcb_connection_t *connection = XGetXCBConnection(display);
    // A keymap read from X11 is self-contained; do not make this protocol
    // probe depend on the host's text keymap database being installed.
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
    if (context) xkb_context_set_log_level(context, XKB_LOG_LEVEL_DEBUG);
    int32_t device = connection
        ? xkb_x11_get_core_keyboard_device_id(connection) : -1;
    struct xkb_keymap *keymap = context && device >= 0
        ? xkb_x11_keymap_new_from_device(context, connection, device,
                                         XKB_KEYMAP_COMPILE_NO_FLAGS)
        : NULL;
    struct xkb_state *state = keymap
        ? xkb_x11_state_new_from_device(keymap, connection, device) : NULL;
    xkb_keycode_t keycode = keymap
        ? xkb_keymap_key_by_name(keymap, "K038") : XKB_KEYCODE_INVALID;
    const xkb_keysym_t *symbols = NULL;
    int symbol_count = keymap && keycode != XKB_KEYCODE_INVALID
        ? xkb_keymap_key_get_syms_by_level(keymap, keycode, 0, 0, &symbols) : 0;
    xkb_keysym_t base_minus = state
        ? xkb_state_key_get_one_sym(state, 20) : XKB_KEY_NoSymbol;
    if (state) xkb_state_update_key(state, 50, XKB_KEY_DOWN);
    xkb_keysym_t shifted_minus = state
        ? xkb_state_key_get_one_sym(state, 20) : XKB_KEY_NoSymbol;
    if (state) xkb_state_update_key(state, 50, XKB_KEY_UP);
    bool ok = connection && context && device == 3 && keymap && state
              && keycode == 38 && symbol_count == 1
              && symbols && symbols[0] == XKB_KEY_a
              && xkb_state_key_get_one_sym(state, keycode) == XKB_KEY_a
              && base_minus == XKB_KEY_minus
              && shifted_minus == XKB_KEY_underscore;
    char detail[160];
    snprintf(detail, sizeof(detail),
             "device=%d keymap=%d state=%d keycode=%u syms=%d first=0x%x minus=0x%x/0x%x",
             device, keymap != NULL, state != NULL, keycode, symbol_count,
             symbols && symbol_count > 0 ? symbols[0] : 0,
             base_minus, shifted_minus);
    result("xkbcommon", ok, detail);
    if (state) xkb_state_unref(state);
    if (keymap) xkb_keymap_unref(keymap);
    if (context) xkb_context_unref(context);
}

static void probe_state_notify(Display *display) {
    Display *peer = XOpenDisplay(NULL);
    if (peer == NULL) {
        result("xkb-state-notify", false, "peer-open");
        return;
    }
    XSetEventQueueOwner(display, XCBOwnsEventQueue);
    xcb_connection_t *connection = XGetXCBConnection(display);
    XFlush(display);
    KeyCode shift = XKeysymToKeycode(peer, XK_Shift_L);
    if (shift == 0)
        shift = 50;
    XTestFakeKeyEvent(peer, shift, True, 0);
    XFlush(peer);
    usleep(20000);
    XTestFakeKeyEvent(peer, shift, False, 0);
    XFlush(peer);

    bool saw_shift_set = false;
    bool saw_shift_clear = false;
    for (int i = 0; i < 40 && !(saw_shift_set && saw_shift_clear); ++i) {
        xcb_generic_event_t *raw_event;
        while ((raw_event = xcb_poll_for_event(connection)) != NULL) {
            if ((raw_event->response_type & 0x7f) == xkb_event_type) {
                xcb_xkb_state_notify_event_t *state_event =
                    (xcb_xkb_state_notify_event_t *)raw_event;
                if (state_event->xkbType == XCB_XKB_STATE_NOTIFY &&
                    state_event->keycode == shift) {
                    if ((state_event->baseMods & ShiftMask) != 0)
                        saw_shift_set = true;
                    else
                        saw_shift_clear = true;
                    printf("BXINPUT xkb-state keycode=%u mods=0x%x base=0x%x\n",
                           state_event->keycode, state_event->mods,
                           state_event->baseMods);
                    fflush(stdout);
                }
            }
            free(raw_event);
        }
        if (!(saw_shift_set && saw_shift_clear))
            usleep(25000);
    }
    char detail[64];
    snprintf(detail, sizeof(detail), "shift-set=%d shift-clear=%d",
             saw_shift_set, saw_shift_clear);
    result("xkb-state-notify", saw_shift_set && saw_shift_clear, detail);
    XCloseDisplay(peer);
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 2;
    XSetErrorHandler(handle_x_error);
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "BXTEST FAIL display-open DISPLAY=%s\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(null)");
        return 2;
    }
    probe_xkb(display);
    probe_xkbcommon(display);
    probe_state_notify(display);
    printf("BXSUMMARY xkb-x11 passed=%d failed=%d xerrors=%d\n",
           passed, checks - passed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 2));
    XCloseDisplay(display);
    return checks == passed && x_errors == 0 ? 0 : 1;
}
