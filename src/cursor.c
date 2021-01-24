#include <inttypes.h>
#include <unistd.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "action.h"
#include "cursor.h"
#include "shell.h"
#include "types.h"


static void process_cursor_move(struct server *server, uint32_t time, double zoom) {
    server->grabbed_view->x = (server->cursor->x - server->grab_x) / zoom;
    server->grabbed_view->y = (server->cursor->y - server->grab_y) / zoom;
}


static void process_cursor_resize(struct server *server, uint32_t time, double zoom) {
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


static bool view_at(
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


static struct view *desktop_view_at(
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


static void process_cursor_motion(struct server *server, uint32_t time, double dx, double dy) {
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
		    .is_percentage = false,
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


static void on_cursor_motion(struct wl_listener *listener, void *data){
    struct server *server = wl_container_of(listener, server, cursor_motion_listener);
    struct wlr_event_pointer_motion *event = data;
    wlr_cursor_move(server->cursor, event->device, event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec, event->delta_x, event->delta_y);
}


static void on_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server,
	cursor_motion_absolute_listener);
    struct wlr_event_pointer_motion_absolute *event = data;
    wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
    process_cursor_motion(server, event->time_msec, 0, 0);
}


static void on_cursor_button(struct wl_listener *listener, void *data) {
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


static void on_cursor_axis(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_axis_listener);
    struct wlr_event_pointer_axis *event = data;
    if (server->cursor_mode == CURSOR_MOD) {
	if (server->on_mouse_scroll) {
	    struct motion motion = {
		.dx = 0,
		.dy = 0,
		.is_percentage = false,
	    };
	    if (event->orientation == WLR_AXIS_ORIENTATION_VERTICAL) {
		motion.dy = server->reverse_scrolling ? - event->delta : event->delta;
	    } else {
		motion.dx = server->reverse_scrolling ? - event->delta : event->delta;
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


static void on_cursor_frame(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, cursor_frame_listener);
    wlr_seat_pointer_notify_frame(server->seat);
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
