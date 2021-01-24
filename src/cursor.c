#include <inttypes.h>
#include <unistd.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "action.h"
#include "cursor.h"
#include "shell.h"
#include "types.h"


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


static bool _view_at(
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


struct view *view_at(
    double lx, double ly, struct wlr_surface **surface, double *sx, double *sy
) {
    struct view *view;
    lx /= wimp.current_desk->zoom;
    ly /= wimp.current_desk->zoom;
    wl_list_for_each(view, &wimp.current_desk->views, link) {
	if (_view_at(view, lx, ly, surface, sx, sy)) {
	    return view;
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
    dx /= zoom;
    dy /= zoom;

    switch (wimp.cursor_mode) {
	case CURSOR_PASSTHROUGH:
	    seat = wimp.seat;
	    surface = NULL;
	    view = view_at(wimp.cursor->x, wimp.cursor->y, &surface, &sx, &sy);
	    if (!view) {
		wlr_xcursor_manager_set_cursor_image(wimp.cursor_manager, "left_ptr", wimp.cursor);
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
	    if (wimp.on_mouse_motion) {
		struct motion motion = {
		    .dx = dx,
		    .dy = dy,
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

    if (event->state == WLR_BUTTON_RELEASED) {
	wimp.cursor_mode = CURSOR_PASSTHROUGH;
    } else {
	double sx, sy;
	struct wlr_surface *surface;
	struct view *view = view_at(
	    wimp.cursor->x, wimp.cursor->y, &surface, &sx, &sy
	);
	focus_view(view, surface);
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


void set_up_cursor() {
    wimp.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(wimp.cursor, wimp.output_layout);
    wimp.cursor_manager = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(wimp.cursor_manager, 1);

    wimp.cursor_motion_listener.notify = on_cursor_motion;
    wl_signal_add(&wimp.cursor->events.motion, &wimp.cursor_motion_listener);

    wimp.cursor_motion_absolute_listener.notify = on_cursor_motion_absolute;
    wl_signal_add(&wimp.cursor->events.motion_absolute,
	&wimp.cursor_motion_absolute_listener);

    wimp.cursor_button_listener.notify = on_cursor_button;
    wl_signal_add(&wimp.cursor->events.button, &wimp.cursor_button_listener);

    wimp.cursor_axis_listener.notify = on_cursor_axis;
    wl_signal_add(&wimp.cursor->events.axis, &wimp.cursor_axis_listener);

    wimp.cursor_frame_listener.notify = on_cursor_frame;
    wl_signal_add(&wimp.cursor->events.frame, &wimp.cursor_frame_listener);
}
