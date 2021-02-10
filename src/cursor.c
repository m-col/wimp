#include <inttypes.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "action.h"
#include "cursor.h"
#include "shell.h"
#include "types.h"


void centre_cursor() {
    struct wlr_box *extents = wlr_output_layout_get_box(wimp.output_layout, NULL);
    double cx, cy;
    wlr_output_layout_closest_point(
	wimp.output_layout, NULL, extents->width / 2, extents->height / 2, &cx, &cy
    );
    wlr_cursor_warp_closest(wimp.cursor, NULL, cx, cy);
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


static void process_cursor_move(uint32_t time, double zoom) {
    wimp.grabbed_view->x = (wimp.cursor->x - wimp.grab_x) / zoom;
    wimp.grabbed_view->y = (wimp.cursor->y - wimp.grab_y) / zoom;
}


static void process_cursor_resize(uint32_t time, double zoom) {
    struct view *view = wimp.grabbed_view;
    int x = wimp.grab_geobox.x;
    int width = wimp.grab_geobox.width;
    int y = wimp.grab_geobox.y;
    int height = wimp.grab_geobox.height;

    double cx = (wimp.cursor->x - wimp.grab_x) / zoom;
    double cy = (wimp.cursor->y - wimp.grab_y) / zoom;

    if (wimp.resize_edges & WLR_EDGE_TOP) {
	height -= cy - y;
	y = cy;
    } else if (wimp.resize_edges & WLR_EDGE_BOTTOM) {
        height += cy - y;
    }
    if (wimp.resize_edges & WLR_EDGE_LEFT) {
        width -= cx - x;
	x = cx;
    } else if (wimp.resize_edges & WLR_EDGE_RIGHT) {
        width += cx - x;
    }

    view->x = x;
    view->y = y;
    wlr_xdg_toplevel_set_size(view->surface, width, height);
}


void *under_pointer(struct wlr_surface **surface, double *sx, double *sy, bool *is_layer) {
    double x = wimp.cursor->x;
    double y = wimp.cursor->y;
    double tsx, tsy;
    struct view *view;
    struct layer_view *lview;
    struct wlr_surface *tsurface;
    struct wlr_output *wlr_output = wlr_output_layout_output_at(wimp.output_layout, x, y);
    struct output *output = wlr_output->data;

    // overlay and top layers
    uint32_t above[] = { ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, ZWLR_LAYER_SHELL_V1_LAYER_TOP };
    size_t nlayers = sizeof(above) / sizeof(above[0]);
    for (size_t i = 0; i < nlayers; i++) {
	wl_list_for_each(lview, &output->layer_views[above[i]], link) {
	    tsurface = wlr_layer_surface_v1_surface_at(
		lview->surface, x - lview->geo.x, y - lview->geo.y, &tsx, &tsy
	    );
	    if (tsurface) {
		*sx = tsx;
		*sy = tsy;
		*surface = tsurface;
		*is_layer = true;
		return lview;
	    }
	}
    }

    // scratchpads
    struct scratchpad *scratchpad;
    wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	if (scratchpad->is_mapped) {
	    view = scratchpad->view;
	    tsurface = wlr_xdg_surface_surface_at(
		view->surface, x - view->x, y - view->y, &tsx, &tsy
	    );
	    if (tsurface) {
		*sx = tsx;
		*sy = tsy;
		*surface = tsurface;
		*is_layer = false;
		return view;
	    }
	}
    }

    // clients
    double zx = x / wimp.current_desk->zoom;
    double zy = y / wimp.current_desk->zoom;

    wl_list_for_each(view, &wimp.current_desk->views, link) {
	tsurface = wlr_xdg_surface_surface_at(
	    view->surface, zx - view->x, zy - view->y, &tsx, &tsy
	);
	if (tsurface) {
	    *sx = tsx;
	    *sy = tsy;
	    *surface = tsurface;
	    *is_layer = false;
	    return view;
	}
    }

    // bottom and background layers
    uint32_t below[] = { ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND };
    for (size_t i = 0; i < nlayers; i++) {
	wl_list_for_each(lview, &output->layer_views[below[i]], link) {
	    tsurface = wlr_layer_surface_v1_surface_at(
		lview->surface, x - lview->geo.x, y - lview->geo.y, &tsx, &tsy
	    );
	    if (tsurface) {
		*sx = tsx;
		*sy = tsy;
		*surface = tsurface;
		*is_layer = true;
		return lview;
	    }
	}
    }

    return NULL;
}


static void process_cursor_motion(uint32_t time, double dx, double dy) {
    double sx, sy;
    struct wlr_seat *seat;
    struct wlr_surface *surface;
    struct view *view;
    double zoom = wimp.current_desk->zoom;

    switch (wimp.cursor_mode) {
	case CURSOR_PASSTHROUGH:
	    seat = wimp.seat;
	    surface = NULL;
	    bool is_layer;
	    view = under_pointer(&surface, &sx, &sy, &is_layer);
	    if (!view) {
		wlr_xcursor_manager_set_cursor_image(wimp.cursor_manager, "left_ptr", wimp.cursor);
	    }
	    if (surface) {
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		if (seat->pointer_state.focused_surface == surface) {
		    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		}
		if (wimp.auto_focus && seat->keyboard_state.focused_surface != surface) {
		    focus_view(view, surface);
		}
	    } else {
		wlr_seat_pointer_clear_focus(seat);
	    }
	    break;

	case CURSOR_MOD:
	    if (wimp.on_mouse_motion) {
		struct motion motion = {
		    .dx = dx / zoom,
		    .dy = dy / zoom,
		    .is_percentage = false,
		};
		wimp.on_mouse_motion(&motion);
	    }
	    break;

	case CURSOR_MOVE:
	    process_cursor_move(time, zoom);
	    break;

	case CURSOR_RESIZE:
	    process_cursor_resize(time, zoom);
	    break;
    }
}


static void on_cursor_motion(struct wl_listener *listener, void *data){
    struct wlr_event_pointer_motion *event = data;
    wlr_cursor_move(wimp.cursor, event->device, event->delta_x, event->delta_y);
    process_cursor_motion(event->time_msec, event->delta_x, event->delta_y);
}


static void on_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_motion_absolute *event = data;
    wlr_cursor_warp_absolute(wimp.cursor, event->device, event->x, event->y);
    process_cursor_motion(event->time_msec, 0, 0);
}


static void on_cursor_button(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_button *event = data;
    wlr_seat_pointer_notify_button(wimp.seat, event->time_msec, event->button, event->state);
    double sx, sy;
    struct wlr_surface *surface;
    bool is_layer;

    if (event->state == WLR_BUTTON_RELEASED) {
	if (wimp.cursor_mode == CURSOR_MOD) {
	    wimp.on_mouse_motion = NULL;
	    struct binding *kb;
	    wl_list_for_each(kb, &wimp.mouse_bindings, link) {
		if (kb->key == MOTION) {
		    wimp.on_mouse_motion = kb->action;
		    break;
		}
	    }
	} else {
	    wimp.cursor_mode = CURSOR_PASSTHROUGH;
	}
	if (wimp.grabbed_view) {
	    wimp.grabbed_view = NULL;
	}
    }

    else if (wimp.cursor_mode == CURSOR_MOD) {
	bool grab = true;
	switch (event->button) {
	    case BTN_LEFT:
		wimp.on_mouse_motion = wimp.on_drag1;
		break;
	    case BTN_MIDDLE:
		wimp.on_mouse_motion = wimp.on_drag2;
		break;
	    case BTN_RIGHT:
		wimp.on_mouse_motion = wimp.on_drag3;
		break;
	    default:
		grab = false;
	}
	if (grab) {
	    wimp.grabbed_view = under_pointer(&surface, &sx, &sy, &is_layer);
	}
    }

    else {
	struct view *view = under_pointer(&surface, &sx, &sy, &is_layer);
	if (!is_layer) {
	    focus_view(view, surface);
	}
    }
}


static void on_cursor_axis(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_axis *event = data;
    if (wimp.cursor_mode == CURSOR_MOD) {
	if (wimp.on_mouse_scroll) {
	    struct motion motion = {
		.dx = 0,
		.dy = 0,
		.is_percentage = false,
	    };
	    if (event->orientation == WLR_AXIS_ORIENTATION_VERTICAL) {
		motion.dy = wimp.reverse_scrolling ? - event->delta : event->delta;
	    } else {
		motion.dx = wimp.reverse_scrolling ? - event->delta : event->delta;
	    };
	    wimp.on_mouse_scroll(&motion);
	}
    } else {
	wlr_seat_pointer_notify_axis(
	    wimp.seat, event->time_msec, event->orientation, event->delta,
	    event->delta_discrete, event->source
	);
    }
}


static void on_cursor_frame(struct wl_listener *listener, void *data) {
    wlr_seat_pointer_notify_frame(wimp.seat);
}


static void on_pinch_end(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_pinch_end *event = data;
    if (wimp.cursor_mode == CURSOR_PASSTHROUGH) {
	wlr_pointer_gestures_v1_send_pinch_end(
	    wimp.pointer_gestures, wimp.seat, event->time_msec, event->cancelled
	);
    }
}


static void on_pinch_update(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_pinch_update *event = data;
    if (wimp.cursor_mode == CURSOR_MOD) {
	if (wimp.on_pinch) {
	    wimp.on_pinch(&event->scale);
	}
    } else {
	wlr_pointer_gestures_v1_send_pinch_update(
	    wimp.pointer_gestures, wimp.seat, event->time_msec,
	    event->dx, event->dy, event->scale, event->rotation
	);
    }
}


static void on_pinch_begin(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_pinch_begin *event = data;
    if (wimp.cursor_mode == CURSOR_MOD) {
	if (wimp.on_pinch_begin) {
	    wimp.on_pinch_begin(NULL);
	}
    } else {
	wlr_pointer_gestures_v1_send_pinch_begin(
	    wimp.pointer_gestures, wimp.seat, event->time_msec, event->fingers
	);
    }
}


static void on_swipe_end(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_swipe_end *event = data;
    wlr_pointer_gestures_v1_send_swipe_end(
	wimp.pointer_gestures, wimp.seat, event->time_msec, event->cancelled
    );
}


static void on_swipe_update(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_swipe_update *event = data;
    wlr_pointer_gestures_v1_send_swipe_update(
	wimp.pointer_gestures, wimp.seat, event->time_msec, event->dx, event->dy
    );
}


static void on_swipe_begin(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_swipe_begin *event = data;
    wlr_pointer_gestures_v1_send_swipe_begin(
	wimp.pointer_gestures, wimp.seat, event->time_msec, event->fingers
    );
}


void set_up_cursor() {
    wimp.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(wimp.cursor, wimp.output_layout);
    wimp.cursor_manager = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(wimp.cursor_manager, 1);

    wimp.request_cursor_listener.notify = on_request_cursor;
    wl_signal_add(&wimp.seat->events.request_set_cursor, &wimp.request_cursor_listener);

    wimp.cursor_axis_listener.notify = on_cursor_axis;
    wimp.cursor_frame_listener.notify = on_cursor_frame;
    wimp.cursor_button_listener.notify = on_cursor_button;
    wimp.cursor_motion_listener.notify = on_cursor_motion;
    wimp.cursor_motion_absolute_listener.notify = on_cursor_motion_absolute;
    wl_signal_add(&wimp.cursor->events.axis, &wimp.cursor_axis_listener);
    wl_signal_add(&wimp.cursor->events.frame, &wimp.cursor_frame_listener);
    wl_signal_add(&wimp.cursor->events.button, &wimp.cursor_button_listener);
    wl_signal_add(&wimp.cursor->events.motion, &wimp.cursor_motion_listener);
    wl_signal_add(&wimp.cursor->events.motion_absolute, &wimp.cursor_motion_absolute_listener);

    wimp.pointer_gestures = wlr_pointer_gestures_v1_create(wimp.display);
    wimp.pinch_end_listener.notify = on_pinch_end;
    wimp.pinch_begin_listener.notify = on_pinch_begin;
    wimp.pinch_update_listener.notify = on_pinch_update;
    wl_signal_add(&wimp.cursor->events.pinch_end, &wimp.pinch_end_listener);
    wl_signal_add(&wimp.cursor->events.pinch_begin, &wimp.pinch_begin_listener);
    wl_signal_add(&wimp.cursor->events.pinch_update, &wimp.pinch_update_listener);
    wimp.swipe_end_listener.notify = on_swipe_end;
    wimp.swipe_begin_listener.notify = on_swipe_begin;
    wimp.swipe_update_listener.notify = on_swipe_update;
    wl_signal_add(&wimp.cursor->events.swipe_end, &wimp.swipe_end_listener);
    wl_signal_add(&wimp.cursor->events.swipe_begin, &wimp.swipe_begin_listener);
    wl_signal_add(&wimp.cursor->events.swipe_update, &wimp.swipe_update_listener);
}
