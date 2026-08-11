#define _GNU_SOURCE
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xcb/xkb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int checks;
static int passed;
static int x_errors;
static int xkb_event_type = -1;

static int handle_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXXERROR code=%u request=%u minor=%u resource=0x%lx %s\n",
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

static void probe_render(Display *display, Window window) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XRenderQueryExtension(display, &event_base, &error_base)) {
        result("xrender", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XRenderQueryVersion(display, &major, &minor) != 0;
    XRenderPictFormat *format = XRenderFindVisualFormat(
        display, DefaultVisual(display, DefaultScreen(display)));
    Picture picture = format ? XRenderCreatePicture(display, window, format, 0, NULL) : 0;
    if (picture) {
        XRenderColor color = {.red = 0x1800, .green = 0xb800,
                              .blue = 0xf000, .alpha = 0xffff};
        XRenderFillRectangle(display, PictOpSrc, picture, &color, 24, 84, 300, 92);
        XRenderFreePicture(display, picture);
    }
    ok = ok && format && picture && sync_without_error(display, before);
    XImage *image = ok ? XGetImage(display, window, 100, 120, 1, 1,
                                   AllPlanes, ZPixmap) : NULL;
    ok = ok && image && XGetPixel(image, 0, 0) != 0;
    if (image) XDestroyImage(image);
    char detail[96];
    snprintf(detail, sizeof(detail), "version=%d.%d event=%d error=%d",
             major, minor, event_base, error_base);
    result("xrender", ok, detail);
}

static void probe_xfixes(Display *display, Window window) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XFixesQueryExtension(display, &event_base, &error_base)) {
        result("xfixes", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XFixesQueryVersion(display, &major, &minor) != 0;
    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    XFixesSelectSelectionInput(display, window, clipboard,
                              XFixesSetSelectionOwnerNotifyMask);
    XSetSelectionOwner(display, clipboard, window, CurrentTime);
    XRectangle source = {.x = 7, .y = 9, .width = 31, .height = 37};
    XserverRegion region = XFixesCreateRegion(display, &source, 1);
    int count = 0;
    XRectangle *fetched = XFixesFetchRegion(display, region, &count);
    ok = ok && region && fetched && count == 1 && fetched[0].x == source.x
         && fetched[0].y == source.y && fetched[0].width == source.width
         && fetched[0].height == source.height;
    XRectangle input = {.x = 0, .y = 0, .width = 360, .height = 420};
    XserverRegion input_region = XFixesCreateRegion(display, &input, 1);
    if (input_region)
        XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, input_region);
    if (fetched) XFree(fetched);
    if (region) XFixesDestroyRegion(display, region);
    if (input_region) XFixesDestroyRegion(display, input_region);
    ok = ok && input_region && sync_without_error(display, before);
    XEvent selection_event = {0};
    bool selection_notify = XCheckTypedEvent(
        display, event_base + XFixesSelectionNotify, &selection_event);
    XFixesSelectionNotifyEvent *notify =
        selection_notify ? (XFixesSelectionNotifyEvent *)&selection_event : NULL;
    selection_notify = selection_notify && notify->subtype == XFixesSetSelectionOwnerNotify
        && notify->window == window && notify->owner == window
        && notify->selection == clipboard;
    ok = ok && selection_notify;
    char detail[128];
    snprintf(detail, sizeof(detail),
             "version=%d.%d rectangles=%d selection-notify=%d",
             major, minor, count, selection_notify);
    result("xfixes", ok, detail);
}

static void probe_randr(Display *display, Window root) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XRRQueryExtension(display, &event_base, &error_base)) {
        result("randr", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XRRQueryVersion(display, &major, &minor) != 0;
    XRRSelectInput(display, root, RRScreenChangeNotifyMask |
                   RRCrtcChangeNotifyMask | RROutputChangeNotifyMask |
                   RROutputPropertyNotifyMask);
    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, root);
    RROutput primary = XRRGetOutputPrimary(display, root);
    XRROutputInfo *output_info = resources && resources->noutput > 0
        ? XRRGetOutputInfo(display, resources, resources->outputs[0]) : NULL;
    XRRCrtcInfo *crtc_info = resources && resources->ncrtc > 0
        ? XRRGetCrtcInfo(display, resources, resources->crtcs[0]) : NULL;
    Atom missing_property = XInternAtom(display, "BIONICX_MISSING_OUTPUT_PROPERTY", False);
    Atom actual_type = None;
    int actual_format = -1;
    unsigned long property_items = 1, bytes_after = 1;
    unsigned char *property_data = NULL;
    int property_status = resources && resources->noutput > 0
        ? XRRGetOutputProperty(display, resources->outputs[0], missing_property,
                               0, 16, False, False, AnyPropertyType,
                               &actual_type, &actual_format, &property_items,
                               &bytes_after, &property_data)
        : BadValue;
    bool primary_found = false;
    for (int i = 0; resources && i < resources->noutput; i++) {
        if (resources->outputs[i] == primary) primary_found = true;
    }
    ok = ok && resources && resources->ncrtc > 0 && resources->noutput > 0
         && resources->nmode > 0
         && primary != None && primary_found
         && resources->modes[0].width
                == (unsigned int)DisplayWidth(display, DefaultScreen(display))
         && resources->modes[0].height
                == (unsigned int)DisplayHeight(display, DefaultScreen(display))
         && resources->modes[0].nameLength > 0
         && output_info && output_info->connection == RR_Connected
         && output_info->crtc == resources->crtcs[0]
         && output_info->mm_width > 0 && output_info->mm_height > 0
         && output_info->nmode == 1 && output_info->npreferred == 1
         && output_info->modes[0] == resources->modes[0].id
         && crtc_info && crtc_info->x == 0 && crtc_info->y == 0
         && crtc_info->width == resources->modes[0].width
         && crtc_info->height == resources->modes[0].height
         && crtc_info->mode == resources->modes[0].id
         && crtc_info->rotation == RR_Rotate_0
         && crtc_info->noutput == 1
         && crtc_info->outputs[0] == resources->outputs[0]
         && property_status == Success && actual_type == None
         && actual_format == 0 && property_items == 0 && bytes_after == 0
         && property_data == NULL
         && sync_without_error(display, before);
    char detail[240];
    snprintf(detail, sizeof(detail),
             "version=%d.%d crtcs=%d outputs=%d primary=0x%lx mode=%dx%d name=%.*s output=%d crtc=%d empty-property=%d",
             major, minor, resources ? resources->ncrtc : 0,
             resources ? resources->noutput : 0,
             (unsigned long)primary,
             resources ? resources->modes[0].width : 0,
             resources ? resources->modes[0].height : 0,
             resources ? resources->modes[0].nameLength : 0,
             resources ? resources->modes[0].name : "",
             output_info != NULL, crtc_info != NULL,
             property_status == Success && actual_type == None);
    if (property_data) XFree(property_data);
    if (crtc_info) XRRFreeCrtcInfo(crtc_info);
    if (output_info) XRRFreeOutputInfo(output_info);
    if (resources) XRRFreeScreenResources(resources);
    result("randr", ok, detail);
}

static void probe_xinput2(Display *display, Window window) {
    int opcode = 0, event_base = 0, error_base = 0;
    if (!XQueryExtension(display, "XInputExtension", &opcode,
                         &event_base, &error_base)) {
        result("xinput2", false, "extension-missing");
        return;
    }
    int before = x_errors;
    int major = 2, minor = 0;
    bool ok = XIQueryVersion(display, &major, &minor) == Success;
    int count = 0;
    XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &count);
    bool master_pointer = false, master_keyboard = false;
    for (int i = 0; devices && i < count; i++) {
        master_pointer |= devices[i].use == XIMasterPointer
                          && devices[i].attachment != devices[i].deviceid;
        master_keyboard |= devices[i].use == XIMasterKeyboard
                           && devices[i].attachment != devices[i].deviceid;
    }
    unsigned char selected_mask[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(selected_mask, XI_KeyPress);
    XISetMask(selected_mask, XI_KeyRelease);
    XISetMask(selected_mask, XI_ButtonPress);
    XISetMask(selected_mask, XI_ButtonRelease);
    XISetMask(selected_mask, XI_Motion);
    XIEventMask selection = {
        .deviceid = XIAllMasterDevices,
        .mask_len = sizeof(selected_mask),
        .mask = selected_mask,
    };
    int select_status = XISelectEvents(display, window, &selection, 1);
    int selected_count = 0;
    XIEventMask *selected = XIGetSelectedEvents(display, window, &selected_count);
    bool selected_ok = false;
    for (int i = 0; selected && i < selected_count; i++) {
        if (selected[i].deviceid == XIAllMasterDevices
            && XIMaskIsSet(selected[i].mask, XI_KeyPress)
            && XIMaskIsSet(selected[i].mask, XI_KeyRelease)
            && XIMaskIsSet(selected[i].mask, XI_ButtonPress)
            && XIMaskIsSet(selected[i].mask, XI_ButtonRelease)
            && XIMaskIsSet(selected[i].mask, XI_Motion)) {
            selected_ok = true;
        }
    }
    ok = ok && devices && count >= 2 && master_pointer && master_keyboard
         && select_status == Success && selected_ok
         && sync_without_error(display, before);
    if (devices) XIFreeDeviceInfo(devices);
    if (selected) XFree(selected);
    char detail[128];
    snprintf(detail, sizeof(detail),
             "version=%d.%d devices=%d masters=%d/%d selected=%d masks=%d",
             major, minor, count, master_pointer, master_keyboard,
             selected_ok, selected_count);
    result("xinput2", ok, detail);
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

static void probe_optional_shm(Display *display) {
    int major = 0, minor = 0;
    Bool pixmaps = False;
    Bool present = XShmQueryVersion(display, &major, &minor, &pixmaps);
    printf("BXCAP mit-shm %s version=%d.%d pixmaps=%d\n",
           present ? "available" : "unavailable", major, minor, pixmaps);
}

int main(int argc, char **argv) {
    int duration = 8;
    if (argc == 3 && strcmp(argv[1], "--duration") == 0) duration = atoi(argv[2]);
    XSetErrorHandler(handle_x_error);
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "BXTEST FAIL display-open DISPLAY=%s\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(null)");
        return 2;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 80, 80, 720, 420, 0,
                                        BlackPixel(display, screen),
                                        BlackPixel(display, screen));
    XStoreName(display, window, "BionicX X11 desktop extension probe");
    XSelectInput(display, window, ExposureMask | StructureNotifyMask | ButtonPressMask);
    XMapWindow(display, window);
    XSync(display, False);

    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, WhitePixel(display, screen));
    const char *title = "BionicX: real glibc desktop X11 extension probe";
    XDrawString(display, window, gc, 24, 42, title, (int)strlen(title));

    probe_render(display, window);
    probe_xfixes(display, window);
    probe_randr(display, root);
    probe_xinput2(display, window);
    probe_xkb(display);
    probe_xkbcommon(display);
    probe_optional_shm(display);

    char summary[128];
    snprintf(summary, sizeof(summary), "desktop extensions: %d/%d strict pass",
             passed, checks);
    XDrawString(display, window, gc, 24, 68, summary, (int)strlen(summary));
    XFlush(display);
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += duration;
    bool saw_shift_set = false;
    bool saw_shift_clear = false;
    int shaped_button_presses = 0;
    xcb_connection_t *event_connection = XGetXCBConnection(display);
    XFlush(display);
    XSetEventQueueOwner(display, XCBOwnsEventQueue);
    while (true) {
        xcb_generic_event_t *raw_event;
        while ((raw_event = xcb_poll_for_event(event_connection)) != NULL) {
            if ((raw_event->response_type & 0x7f) == xkb_event_type) {
                xcb_xkb_state_notify_event_t *state_event =
                    (xcb_xkb_state_notify_event_t *)raw_event;
                if (state_event->xkbType == XCB_XKB_STATE_NOTIFY &&
                    state_event->keycode == 50) {
                    if ((state_event->baseMods & ShiftMask) != 0)
                        saw_shift_set = true;
                    else
                        saw_shift_clear = true;
                    printf("BXINPUT xkb-state keycode=%u event=%d mods=0x%x base=0x%x locked=0x%x changed=0x%x\n",
                           state_event->keycode, state_event->eventType,
                           state_event->mods, state_event->baseMods,
                           state_event->lockedMods, state_event->changed);
                    fflush(stdout);
                }
            }
            else if ((raw_event->response_type & 0x7f) == XCB_BUTTON_PRESS) {
                xcb_button_press_event_t *button = (xcb_button_press_event_t *)raw_event;
                if (button->event == window) {
                    shaped_button_presses++;
                    printf("BXINPUT xfixes-input-shape event-x=%d event-y=%d root-x=%d root-y=%d\n",
                           button->event_x, button->event_y,
                           button->root_x, button->root_y);
                    fflush(stdout);
                }
            }
            free(raw_event);
        }
        struct timespec now = {0};
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) break;
        struct timespec pause = {.tv_nsec = 20 * 1000 * 1000};
        nanosleep(&pause, NULL);
    }
    char state_detail[96];
    snprintf(state_detail, sizeof(state_detail), "shift-set=%d shift-clear=%d",
             saw_shift_set, saw_shift_clear);
    result("xkb-state-notify", saw_shift_set && saw_shift_clear, state_detail);
    char shape_detail[96];
    snprintf(shape_detail, sizeof(shape_detail), "inside-presses=%d expected=1",
             shaped_button_presses);
    result("xfixes-input-shape", shaped_button_presses == 1, shape_detail);
    printf("BXSUMMARY desktop-x11 passed=%d failed=%d xerrors=%d\n",
           passed, checks - passed, x_errors);
    fflush(stdout);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return checks == passed ? 0 : 1;
}
