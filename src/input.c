#include <inttypes.h>
#include <unistd.h>
#include <wayland-server-protocol.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>

#include "action.h"
#include "cursor.h"
#include "output.h"
#include "shell.h"
#include "types.h"


static void on_modifier(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, modifier_listener);
    wlr_seat_keyboard_notify_modifiers(
	wimp.seat, &keyboard->device->keyboard->modifiers
    );

    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
    bool handled = false;
    struct binding *kb;
    if ((modifiers & wimp.mod)) {
	modifiers &= ~wimp.mod;
	wl_list_for_each(kb, &wimp.mouse_bindings, link) {
	    if (modifiers == kb->mods) {
		switch (kb->key) {
		    case MOTION:
			wimp.on_mouse_motion = kb->action;
			break;
		    case SCROLL:
			wimp.on_mouse_scroll = kb->action;
			break;
		    case DRAG1:
			wimp.on_drag1 = kb->action;
			break;
		    case DRAG2:
			wimp.on_drag2 = kb->action;
			break;
		    case DRAG3:
			wimp.on_drag3 = kb->action;
			break;
		    case PINCH:
			wimp.on_pinch = kb->action;
			wimp.on_pinch_begin = kb->begin;
			break;
		}
		wimp.cursor_mode = CURSOR_MOD;
		handled = true;
	    }
	}
	if (!wimp.mod_on) {
	    wimp.mod_on = true;
	    damage_all_views();
	}
    } else {
	if (wimp.mod_on) {
	    wimp.mod_on = false;
	    damage_all_views();
	}
    }

    if (!handled){
	wimp.cursor_mode = CURSOR_PASSTHROUGH;
    }
}


static void on_key(struct wl_listener *listener, void *data) {
    struct wlr_event_keyboard_key *event = data;

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
	const xkb_keysym_t *syms;
	xkb_keycode_t keycode = event->keycode + 8;
	struct keyboard *keyboard = wl_container_of(listener, keyboard, key_listener);
	struct wlr_keyboard *wlr_kb = keyboard->device->keyboard;

	if (wimp.mark_waiting) {
	    struct mark *mark;
	    wimp.mark_waiting = false;
	    damage_mark_indicator();
	    xkb_state_key_get_syms(wlr_kb->xkb_state, keycode, &syms);
	    mark = wl_container_of(wimp.marks.next, mark, link);
	    if (mark->key) {
		actually_go_to_mark(syms[0]);
	    } else {
		actually_set_mark(syms[0]);
	    }
	    return;
	}

	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	struct binding *kb;
	if ((modifiers & wimp.mod)) {
	    xkb_layout_index_t layout_index = xkb_state_key_get_layout(wlr_kb->xkb_state, keycode);
	    int nsyms = xkb_keymap_key_get_syms_by_level(
		wlr_kb->keymap, keycode, layout_index, 0, &syms
	    );

	    modifiers &= ~wimp.mod;
	    for (int i = 0; i < nsyms; i++) {
		wl_list_for_each(kb, &wimp.key_bindings, link) {
		    if (syms[i] == kb->key && modifiers == kb->mods) {
			kb->action(kb->data);
			return;
		    }
		}
	    }
	}
    }

    // forward to client
    wlr_seat_keyboard_notify_key(
	wimp.seat, event->time_msec, event->keycode, event->state
    );
}


static void on_keyboard_destroy(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, destroy_listener);

    wl_list_remove(&keyboard->link);
    wl_list_remove(&keyboard->modifier_listener.link);
    wl_list_remove(&keyboard->key_listener.link);
    wl_list_remove(&keyboard->destroy_listener.link);
    free(keyboard);

    if (!wl_list_empty(&wimp.keyboards)) {
	keyboard = wl_container_of(wimp.keyboards.next, keyboard, link);
	wlr_seat_set_keyboard(wimp.seat, keyboard->device);
    }
}


static void add_new_pointer(struct wlr_input_device *device) {
    if (wlr_input_device_is_libinput(device)) {
	struct libinput_device *ldev = wlr_libinput_get_device_handle(device);
	if (libinput_device_config_tap_get_finger_count(ldev) > 1) {
	    libinput_device_config_tap_set_enabled(ldev, LIBINPUT_CONFIG_TAP_ENABLED);
	}
    }
    wlr_cursor_attach_input_device(wimp.cursor, device);
}


static void add_new_keyboard(struct wlr_input_device *device) {
    struct keyboard *keyboard = calloc(1, sizeof(struct keyboard));
    keyboard->device = device;

    // currently assumes default "us"
    struct xkb_rule_names rules = { 0 };
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
	XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(device->keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

    keyboard->key_listener.notify = on_key;
    keyboard->destroy_listener.notify = on_keyboard_destroy;
    keyboard->modifier_listener.notify = on_modifier;
    wl_signal_add(&device->keyboard->events.key, &keyboard->key_listener);
    wl_signal_add(&device->keyboard->events.destroy, &keyboard->destroy_listener);
    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifier_listener);

    wlr_seat_set_keyboard(wimp.seat, device);
    wl_list_insert(&wimp.keyboards, &keyboard->link);
}


static void on_request_set_selection(struct wl_listener *listener, void *data) {
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(wimp.seat, event->source, event->serial);
}


static void on_new_input(struct wl_listener *listener, void *data) {
    struct wlr_input_device *device = data;
    switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
	    add_new_keyboard(device);
	    break;
	case WLR_INPUT_DEVICE_POINTER:
	    add_new_pointer(device);
	    break;
	default:
	    break;
    }

    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&wimp.keyboards))
	capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(wimp.seat, capabilities);
}


static void on_new_virtual_keyboard(struct wl_listener *listener, void *data) {
    struct wlr_virtual_keyboard_v1 *keyboard = data;
    struct wlr_input_device *device = &keyboard->input_device;
    add_new_keyboard(device);
}


void set_up_inputs() {
    wimp.seat = wlr_seat_create(wimp.display, "seat0");
    wlr_data_device_manager_create(wimp.display);
    wlr_primary_selection_v1_device_manager_create(wimp.display);
    wl_list_init(&wimp.keyboards);

    wimp.new_input_listener.notify = on_new_input;
    wl_signal_add(&wimp.backend->events.new_input, &wimp.new_input_listener);

    wimp.request_set_selection_listener.notify = on_request_set_selection;
    wl_signal_add(&wimp.seat->events.request_set_selection,
	&wimp.request_set_selection_listener);

    wimp.virtual_keyboard = wlr_virtual_keyboard_manager_v1_create(wimp.display);
    wimp.new_virtual_keyboard_listener.notify = on_new_virtual_keyboard;
    wl_signal_add(&wimp.virtual_keyboard->events.new_virtual_keyboard,
	&wimp.new_virtual_keyboard_listener);
}
