#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "input.h"
#include "shell.h"
#include "types.h"


enum wlr_keyboard_modifier MOD = WLR_MODIFIER_LOGO;


void process_cursor_move(struct server *server, uint32_t time) {
    server->grabbed_view->x = server->cursor->x - server->grab_x;
    server->grabbed_view->y = server->cursor->y - server->grab_y;
}


void process_cursor_resize(struct server *server, uint32_t time) {
    struct view *view = server->grabbed_view;
    double border_x = server->cursor->x - server->grab_x;
    double border_y = server->cursor->y - server->grab_y;
    int new_left = server->grab_geobox.x;
    int new_right = server->grab_geobox.x + server->grab_geobox.width;
    int new_top = server->grab_geobox.y;
    int new_bottom = server->grab_geobox.y + server->grab_geobox.height; 

    if (server->resize_edges & WLR_EDGE_TOP) {
        new_top = border_y;
        if (new_top >= new_bottom) {
            new_top = new_bottom - 1;
        }
    } else if (server->resize_edges & WLR_EDGE_BOTTOM) {
        new_bottom = border_y;
        if (new_bottom <= new_top) {
            new_bottom = new_top + 1;
        }
    }
    if (server->resize_edges & WLR_EDGE_LEFT) {
        new_left = border_x;
        if (new_left >= new_right) {
            new_left = new_right - 1;
        }
    } else if (server->resize_edges & WLR_EDGE_RIGHT) {
        new_right = border_x;
        if (new_right <= new_left) {
            new_right = new_left + 1;
        }
    }

    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(view->surface, &geo_box);
    view->x = new_left - geo_box.x;
    view->y = new_top - geo_box.y;

    int new_width = new_right - new_left;
    int new_height = new_bottom - new_top;
    wlr_xdg_toplevel_set_size(view->surface, new_width, new_height);
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
    wl_list_for_each(view, &server->views, link) {
	if (view_at(view, lx, ly, surface, sx, sy)) {
	    return view;
	}
    }
    return NULL;
}


void process_cursor_motion(struct server *server, uint32_t time) {
    double sx, sy;
    struct wlr_seat *seat;
    struct wlr_surface *surface;
    struct view *view;

    switch (server->cursor_mode) {
	case CURSOR_MOVE:
	    process_cursor_move(server, time);
	    break;
	case CURSOR_RESIZE:
	    process_cursor_resize(server, time);
	    break;
	case CURSOR_PASSTHROUGH:
	    seat = server->seat;
	    surface = NULL;
	    view = desktop_view_at(server, server->cursor->x,
		server->cursor->y, &surface, &sx, &sy);
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
    }
}


void on_cursor_motion(struct wl_listener *listener, void *data){
    struct server *server = wl_container_of(listener, server, cursor_motion_listener);
    struct wlr_event_pointer_motion *event = data;
    wlr_cursor_move(server->cursor, event->device, event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec);
}


void on_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server,
	cursor_motion_absolute_listener);
    struct wlr_event_pointer_motion_absolute *event = data;
    wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}


void on_cursor_button(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_button_listener);
    struct wlr_event_pointer_button *event = data;
    wlr_seat_pointer_notify_button(
	server->seat, event->time_msec, event->button, event->state
    );
    double sx, sy;
    struct wlr_surface *surface;
    struct view *view = desktop_view_at(
	server, server->cursor->x, server->cursor->y, &surface, &sx, &sy
    );

    if (event->state == WLR_BUTTON_RELEASED) {
	server->cursor_mode = CURSOR_PASSTHROUGH;
    } else {
	focus_view(view, surface);
    }
}


void on_cursor_axis(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_axis_listener);
    struct wlr_event_pointer_axis *event = data;
    wlr_seat_pointer_notify_axis(
	server->seat, event->time_msec, event->orientation, event->delta,
	event->delta_discrete, event->source
    );
}


void on_cursor_frame(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_frame_listener);
    wlr_seat_pointer_notify_frame(server->seat);
}


void on_modifier(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, modifier_listener);
    wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device); // move to on_new_keyboard?
    wlr_seat_keyboard_notify_modifiers(
	keyboard->server->seat, &keyboard->device->keyboard->modifiers
    );
}


bool handle_keybinding(struct server *server, xkb_keysym_t sym) {
    switch (sym) {

    case XKB_KEY_Escape:
	wl_display_terminate(server->display);
	break;

    case XKB_KEY_F1: // to next view
	if (wl_list_length(&server->views) < 2) {
	    break;
	}
	struct view *current_view = wl_container_of(
	    server->views.next, current_view, link);
	struct view *next_view = wl_container_of(
	    current_view->link.next, next_view, link);
	focus_view(next_view, next_view->surface->surface);
	wl_list_remove(&current_view->link);
	wl_list_insert(server->views.prev, &current_view->link);
	break;

    default:
	return false;
    }
    return true;
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
    if ((modifiers & MOD) && event->state == WLR_KEY_PRESSED) {
	for (int i = 0; i < nsyms; i++) {
		handled = handle_keybinding(server, syms[i]);
	}
    }

    if (!handled) {
	// forward to client
	wlr_seat_set_keyboard(seat, keyboard->device); // move to on_new_keyboard?
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
