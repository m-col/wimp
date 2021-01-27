#include <inttypes.h>
#include <unistd.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_data_device.h>

#include "action.h"
#include "cursor.h"
#include "shell.h"
#include "types.h"


static void on_modifier(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, modifier_listener);
    wlr_seat_keyboard_notify_modifiers(
	wimp.seat, &keyboard->device->keyboard->modifiers
    );

    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
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
		}
		wimp.cursor_mode = CURSOR_MOD;
		return;
	    }
	}
    }

    wimp.cursor_mode = CURSOR_PASSTHROUGH;
}


static void on_key(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, key_listener);
    struct wlr_event_keyboard_key *event = data;
    struct wlr_seat *seat = wimp.seat;

    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(
	keyboard->device->keyboard->xkb_state,
	event->keycode + 8,
	&syms
    );

    if (event->state == WLR_KEY_PRESSED) {
	if (wimp.mark_waiting) {
	    wimp.mark_waiting = false;
	    struct mark *mark;
	    mark = wl_container_of(wimp.marks.next, mark, link);
	    if (mark->key == 0) {
		actually_set_mark(syms[0]);
	    } else {
		actually_go_to_mark(syms[0]);
	    }
	    return;
	}

	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	struct binding *kb;
	if ((modifiers & wimp.mod)) {
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
	seat, event->time_msec, event->keycode, event->state
    );
}


static void on_new_keyboard(struct wlr_input_device *device) {
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

    keyboard->modifier_listener.notify = on_modifier;
    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifier_listener);
    keyboard->key_listener.notify = on_key;
    wl_signal_add(&device->keyboard->events.key, &keyboard->key_listener);
    wlr_seat_set_keyboard(wimp.seat, device);
    wl_list_insert(&wimp.keyboards, &keyboard->link);
}


static void on_request_set_selection(struct wl_listener *listener, void *data) {
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(wimp.seat, event->source, event->serial);
}


static void on_request_cursor(struct wl_listener *listener, void *data) {
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused_client =
	wimp.seat->pointer_state.focused_client;

    if (focused_client == event->seat_client) {
	wlr_cursor_set_surface(
	    wimp.cursor, event->surface, event->hotspot_x, event->hotspot_y
	);
    }
}


static void on_new_input(struct wl_listener *listener, void *data) {
    struct wlr_input_device *device = data;
    switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
	    on_new_keyboard(device);
	    break;
	case WLR_INPUT_DEVICE_POINTER:
	    wlr_cursor_attach_input_device(wimp.cursor, device);
	    break;
	default:
	    break;
    }

    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&wimp.keyboards))
	capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(wimp.seat, capabilities);
}


void set_up_keyboard() {
    wl_list_init(&wimp.keyboards);

    wimp.new_input_listener.notify = on_new_input;
    wl_signal_add(&wimp.backend->events.new_input, &wimp.new_input_listener);

    wimp.seat = wlr_seat_create(wimp.display, "seat0");
    wimp.request_cursor_listener.notify = on_request_cursor;
    wl_signal_add(&wimp.seat->events.request_set_cursor, &wimp.request_cursor_listener);

    wimp.request_set_selection_listener.notify = on_request_set_selection;
    wl_signal_add(&wimp.seat->events.request_set_selection,
	&wimp.request_set_selection_listener);
}
