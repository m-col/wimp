#include <inttypes.h>
#include <unistd.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "input.h"
#include "shell.h"
#include "types.h"

#define is_int(s) (strspn(s, "-0123456789") == strlen(s))


void process_cursor_move(struct server *server, uint32_t time, double zoom) {
    server->grabbed_view->x = (server->cursor->x - server->grab_x) / zoom;
    server->grabbed_view->y = (server->cursor->y - server->grab_y) / zoom;
}


void process_cursor_resize(struct server *server, uint32_t time, double zoom) {
    struct view *view = server->grabbed_view;
    int x = server->grab_geobox.x;
    int width = server->grab_geobox.width;
    int y = server->grab_geobox.y;
    int height = server->grab_geobox.height;

    double cx = (server->cursor->x - server->grab_x) / zoom;
    double cy = (server->cursor->y - server->grab_y) / zoom;

    if (server->resize_edges & WLR_EDGE_TOP) {
	height -= cy - y;
	y = cy;
    } else if (server->resize_edges & WLR_EDGE_BOTTOM) {
        height += cy - y;
    }
    if (server->resize_edges & WLR_EDGE_LEFT) {
        width -= cx - x;
	x = cx;
    } else if (server->resize_edges & WLR_EDGE_RIGHT) {
        width += cx - x;
    }

    view->x = x;
    view->y = y;
    wlr_xdg_toplevel_set_size(view->surface, width, height);
}


bool view_at(
    struct view *view, double lx, double ly, struct wlr_surface **surface,
    double *sx, double *sy
) {
    double view_sx = lx - view->x;
    double view_sy = ly - view->y;

    double _sx, _sy;
    struct wlr_surface *_surface = NULL;
    _surface = wlr_xdg_surface_surface_at(
	view->surface, view_sx, view_sy, &_sx, &_sy
    );
    if (_surface != NULL) {
	*sx = _sx;
	*sy = _sy;
	*surface = _surface;
	return true;
    }
    return false;
}


struct view *desktop_view_at(
    struct server *server, double lx, double ly, struct wlr_surface **surface,
    double *sx, double *sy
) {
    struct view *view;
    wl_list_for_each(view, &server->current_desk->views, link) {
	if (view_at(view, lx, ly, surface, sx, sy)) {
	    return view;
	}
    }
    return NULL;
}


void process_cursor_motion(struct server *server, uint32_t time, double dx, double dy) {
    double sx, sy;
    struct wlr_seat *seat;
    struct wlr_surface *surface;
    struct view *view;
    double zoom = server->current_desk->zoom;
    dx /= zoom;
    dy /= zoom;

    switch (server->cursor_mode) {
	case CURSOR_PASSTHROUGH:
	    seat = server->seat;
	    surface = NULL;
	    view = desktop_view_at(server, server->cursor->x / zoom,
		server->cursor->y / zoom, &surface, &sx, &sy);
	    if (!view) {
		wlr_xcursor_manager_set_cursor_image(
		    server->cursor_manager, "left_ptr", server->cursor
		);
	    }
	    if (surface) {
		bool focus_changed = seat->pointer_state.focused_surface != surface;
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		if (!focus_changed) {
		    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		}
	    } else {
		wlr_seat_pointer_clear_focus(seat);
	    }
	    break;

	case CURSOR_MOD:
	    if (server->on_mouse_motion) {
		struct motion motion = {
		    .dx = dx,
		    .dy = dy,
		};
		server->on_mouse_motion(server, &motion);
	    }
	    break;

	case CURSOR_MOVE:
	    process_cursor_move(server, time, zoom);
	    break;

	case CURSOR_RESIZE:
	    process_cursor_resize(server, time, zoom);
	    break;
    }
}


void on_cursor_motion(struct wl_listener *listener, void *data){
    struct server *server = wl_container_of(listener, server, cursor_motion_listener);
    struct wlr_event_pointer_motion *event = data;
    wlr_cursor_move(server->cursor, event->device, event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec, event->delta_x, event->delta_y);
}


void on_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server,
	cursor_motion_absolute_listener);
    struct wlr_event_pointer_motion_absolute *event = data;
    wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
    process_cursor_motion(server, event->time_msec, 0, 0);
}


void on_cursor_button(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_button_listener);
    struct wlr_event_pointer_button *event = data;
    wlr_seat_pointer_notify_button(
	server->seat, event->time_msec, event->button, event->state
    );

    if (event->state == WLR_BUTTON_RELEASED) {
	server->cursor_mode = CURSOR_PASSTHROUGH;
    } else {
	double sx, sy;
	struct wlr_surface *surface;
	struct view *view = desktop_view_at(
	    server, server->cursor->x, server->cursor->y, &surface, &sx, &sy
	);
	focus_view(view, surface);
    }
}


void on_cursor_axis(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_axis_listener);
    struct wlr_event_pointer_axis *event = data;
    if (server->cursor_mode == CURSOR_MOD) {
	if (server->on_mouse_scroll) {
	    struct motion motion = {
		.dx = 0,
		.dy = 0,
	    };
	    if (event->orientation == WLR_AXIS_ORIENTATION_VERTICAL) {
		motion.dy = server->reverse_scrolling ? event->delta : - event->delta;
	    } else {
		motion.dx = server->reverse_scrolling ? event->delta : - event->delta;
	    };
	    server->on_mouse_scroll(server, &motion);
	}
    } else {
	wlr_seat_pointer_notify_axis(
	    server->seat, event->time_msec, event->orientation, event->delta,
	    event->delta_discrete, event->source
	);
    }
}


void on_cursor_frame(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_frame_listener);
    wlr_seat_pointer_notify_frame(server->seat);
}


void on_modifier(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, modifier_listener);
    struct server *server = keyboard->server;
    wlr_seat_keyboard_notify_modifiers(
	server->seat, &keyboard->device->keyboard->modifiers
    );

    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
    struct binding *kb;
    bool handled = false;
    if ((modifiers & server->mod)) {
	modifiers &= ~server->mod;
	wl_list_for_each(kb, &server->mouse_bindings, link) {
	    if (modifiers == kb->mods) {
		switch (kb->key) {
		    case MOTION:
			printf("MOTION\n");
			server->on_mouse_motion = kb->action;
			break;
		    case SCROLL:
			server->on_mouse_scroll = kb->action;
			break;
		}
		keyboard->server->cursor_mode = CURSOR_MOD;
		handled = true;
	    }
	}
    }
    if (!handled)
	keyboard->server->cursor_mode = CURSOR_PASSTHROUGH;
}


void on_key(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, key_listener);
    struct server *server = keyboard->server;
    struct wlr_event_keyboard_key *event = data;
    struct wlr_seat *seat = server->seat;

    /* Translate libinput keycode -> xkbcommon */
    uint32_t keycode = event->keycode + 8;
    /* Get a list of keysyms based on the keymap for this keyboard */
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(
	keyboard->device->keyboard->xkb_state, keycode, &syms);

    bool handled = false;
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
    struct binding *kb;
    if ((modifiers & server->mod) && event->state == WLR_KEY_PRESSED) {
	modifiers &= ~server->mod;
	for (int i = 0; i < nsyms; i++) {
	    wl_list_for_each(kb, &server->key_bindings, link) {
		if (syms[i] == kb->key && modifiers == kb->mods) {
		    kb->action(server, kb->data);
		    handled = true;
		}
	    }
	}
    }

    if (!handled) {
	// forward to client
	wlr_seat_keyboard_notify_key(seat, event->time_msec,
	    event->keycode, event->state);
    }
}


void on_new_keyboard(struct server *server, struct wlr_input_device *device) {
    struct keyboard *keyboard = calloc(1, sizeof(struct keyboard));
    keyboard->server = server;
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
    wlr_seat_set_keyboard(server->seat, device);
    wl_list_insert(&server->keyboards, &keyboard->link);
}


void on_new_input(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(
	listener, server, new_input_listener
    );

    struct wlr_input_device *device = data;
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
	    on_new_keyboard(server, device);
	    break;
    case WLR_INPUT_DEVICE_POINTER:
	    wlr_cursor_attach_input_device(server->cursor, device);
	    break;
    default:
	    break;
    }
    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) {
	    capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server->seat, capabilities);
}


void on_request_cursor(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, request_cursor_listener);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused_client =
	server->seat->pointer_state.focused_client;

    if (focused_client == event->seat_client) {
	wlr_cursor_set_surface(
	    server->cursor, event->surface, event->hotspot_x, event->hotspot_y
	);
    }
}


void on_request_set_selection(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(
	listener, server, request_set_selection_listener);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}


void set_up_cursor(struct server *server) {
    server->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server->cursor, server->output_layout);
    server->cursor_manager = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(server->cursor_manager, 1);

    server->cursor_motion_listener.notify = on_cursor_motion;
    wl_signal_add(&server->cursor->events.motion, &server->cursor_motion_listener);

    server->cursor_motion_absolute_listener.notify = on_cursor_motion_absolute;
    wl_signal_add(&server->cursor->events.motion_absolute,
	&server->cursor_motion_absolute_listener);

    server->cursor_button_listener.notify = on_cursor_button;
    wl_signal_add(&server->cursor->events.button, &server->cursor_button_listener);

    server->cursor_axis_listener.notify = on_cursor_axis;
    wl_signal_add(&server->cursor->events.axis, &server->cursor_axis_listener);

    server->cursor_frame_listener.notify = on_cursor_frame;
    wl_signal_add(&server->cursor->events.frame, &server->cursor_frame_listener);
}


void set_up_keyboard(struct server *server) {
    wl_list_init(&server->keyboards);

    server->new_input_listener.notify = on_new_input;
    wl_signal_add(&server->backend->events.new_input, &server->new_input_listener);

    server->seat = wlr_seat_create(server->display, "seat0");
    server->request_cursor_listener.notify = on_request_cursor;
    wl_signal_add(&server->seat->events.request_set_cursor,
	    &server->request_cursor_listener);

    server->request_set_selection_listener.notify = on_request_set_selection;
    wl_signal_add(&server->seat->events.request_set_selection,
		    &server->request_set_selection_listener);
}
