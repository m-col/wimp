#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>

#include "action.h"
#include "types.h"


static struct scratchpad *scratchpad_from_view(struct view *view) {
    struct scratchpad *scratchpad;

    wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	if (view == scratchpad->view) {
	    return scratchpad;
	}
    }

    return NULL;
}


static void scratchpad_apply_geo(struct scratchpad *scratchpad) {
    struct wlr_output *output =
	wlr_output_layout_output_at(wimp.output_layout, wimp.cursor->x, wimp.cursor->y);
    struct wlr_box *ogeo = wlr_output_layout_get_box(wimp.output_layout, output);
    struct view *view = scratchpad->view;
    struct wlr_box *geo = &scratchpad->geo;

    if (geo->x > 0) {
	view->x = geo->x;
    } else {
	view->x = ogeo->x - (ogeo->width * geo->x / 100);
    }
    if (geo->y > 0) {
	view->y = geo->y;
    } else {
	view->y = ogeo->y - (ogeo->height * geo->y / 100);
    }

    struct wlr_box *sgeo = &view->surface->geometry;
    if (geo->width > 0) {
	sgeo->width = geo->width;
    } else {
	sgeo->width = - ogeo->width * geo->width / 100;
    }
    if (geo->height > 0) {
	sgeo->height = geo->height;
    } else {
	sgeo->height = - ogeo->height * geo->height / 100;
    }
    wlr_xdg_toplevel_set_size(view->surface, sgeo->width, sgeo->height);
}


void unmap_view(struct view *view) {
    wl_signal_emit(&view->surface->events.unmap, view);
}


void map_view(struct view *view) {
    wl_signal_emit(&view->surface->events.map, view);
}


void focus_view(struct view *view, struct wlr_surface *surface) {
    if (view == NULL) {
	return;
    }
    struct wlr_seat *seat = wimp.seat;
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
    if (prev_surface == surface) {
	return;
    }
    if (prev_surface && wlr_surface_is_xdg_surface(prev_surface)) {
	struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
	    seat->keyboard_state.focused_surface
	);
	wlr_xdg_toplevel_set_activated(previous, false);
    }
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

    if (view->is_scratchpad) {
	struct scratchpad *scratchpad;
	wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	    if (scratchpad->view == view) {
		scratchpad->is_mapped = true;
	    }
	}
    } else {
	wl_list_remove(&view->link);
	wl_list_insert(&wimp.current_desk->views, &view->link);
    }

    wlr_xdg_toplevel_set_activated(view->surface, true);
    wlr_seat_keyboard_notify_enter(
	seat, view->surface->surface, keyboard->keycodes,
	keyboard->num_keycodes, &keyboard->modifiers
    );
}


void pan_to_view(struct view *view) {
    int border_width = wimp.current_desk->border_width;
    double x = view->x - border_width;
    double y = view->y - border_width;
    double width = view->surface->geometry.width + border_width * 2;
    double height = view->surface->geometry.height + border_width * 2;
    struct wlr_box *extents = wlr_output_layout_get_box(wimp.output_layout, NULL);
    double zoom = wimp.current_desk->zoom;
    struct motion motion = {
	.dx = 0,
	.dy = 0,
	.is_percentage = false,
    };

    if (x < 0) {
	motion.dx = x * zoom;
    } else if (x + width > extents->width / zoom) {
	motion.dx = (x + width) * zoom - extents->width;
    }
    if (y < 0) {
	motion.dy = y * zoom;
    } else if (y + height > extents->height / zoom) {
	motion.dy = (y + height) * zoom - extents->height;
    }
    pan_desk(&motion);
}


void fullscreen_xdg_surface(
    struct view *view, struct wlr_xdg_surface *xdg_surface, struct wlr_output *output
) {
    /* output can be NULL, in which case it is calculated using the surface's position */
    struct wlr_box *saved_geo = &wimp.current_desk->fullscreened_saved_geo;

    struct wlr_xdg_surface *prev_surface = wimp.current_desk->fullscreened;
    if (prev_surface) {
	wlr_xdg_toplevel_set_fullscreen(prev_surface, false);
	wlr_xdg_toplevel_set_size(prev_surface, saved_geo->width, saved_geo->height);
	view->x = saved_geo->x;
	view->y = saved_geo->y;
    }

    if (prev_surface == xdg_surface) {
	wimp.current_desk->fullscreened = NULL;
	return;
    }

    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
	return;

    if (!output) {
	double x = view->x + xdg_surface->geometry.width / 2;
	double y = view->y + xdg_surface->geometry.height / 2;
	double lx, ly;
	wlr_output_layout_closest_point(wimp.output_layout, NULL, x, y, &lx, &ly);
	output = wlr_output_layout_output_at(wimp.output_layout, lx, ly);
    }

    wimp.current_desk->fullscreened = xdg_surface;
    saved_geo->x = view->x;
    saved_geo->y = view->y;
    view->x = view->y = 0;
    saved_geo->width = xdg_surface->geometry.width;
    saved_geo->height = xdg_surface->geometry.height;
    wlr_xdg_toplevel_set_fullscreen(xdg_surface, true);
    double zoom = wimp.current_desk->zoom;
    wlr_xdg_toplevel_set_size(xdg_surface, output->width / zoom, output->height / zoom);
}


void unfullscreen() {
    if (wimp.current_desk->fullscreened != NULL) {
	toggle_fullscreen(NULL);
    }
}


static void on_map(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, map_listener);

    if (view->is_scratchpad) {
	struct scratchpad *scratchpad = scratchpad_from_view(view);
	scratchpad->is_mapped = true;
	scratchpad_apply_geo(scratchpad);
    }

    if (wimp.can_steal_focus) {
	focus_view(view, view->surface->surface);
    }
}

static void on_unmap(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, unmap_listener);

    if (view->is_scratchpad) {
	struct scratchpad *scratchpad = scratchpad_from_view(view);
	scratchpad->is_mapped = false;
    } else {
	wl_list_remove(&view->link);
	wl_list_insert(wimp.current_desk->views.prev, &view->link);
    }

    if (wimp.can_steal_focus) {
	if (!wl_list_empty(&wimp.current_desk->views)) {
	    struct view *next_view = wl_container_of(wimp.current_desk->views.next, view, link);
	    focus_view(next_view, next_view->surface->surface);
	}
    }
}


static void on_surface_destroy(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, destroy_listener);
    if (wimp.current_desk->fullscreened == view->surface) {
	wimp.current_desk->fullscreened = NULL;
    }

    if (view->is_scratchpad) {
	struct scratchpad *scratchpad = scratchpad_from_view(view);
	scratchpad->is_mapped = false;
	scratchpad->view = NULL;
    } else {
	wl_list_remove(&view->link);
    }

    free(view);
}


static void process_move_resize(struct view *view, enum cursor_mode mode, uint32_t edges) {
    struct wlr_surface *focused_surface = wimp.seat->pointer_state.focused_surface;

    if (view->surface->surface != focused_surface) {
	    return;
    }
    wimp.grabbed_view = view;
    wimp.cursor_mode = mode;

    double zoom = wimp.current_desk->zoom;
    wimp.grab_x = wimp.cursor->x - view->x * zoom;
    wimp.grab_y = wimp.cursor->y - view->y * zoom;

    if (mode == CURSOR_RESIZE) {
	wlr_xdg_surface_get_geometry(view->surface, &wimp.grab_geobox);
	wimp.grab_geobox.x = view->x;
	wimp.grab_geobox.y = view->y;
	wimp.resize_edges = edges;
    }
}


static void on_request_move(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, request_move_listener);
    wlr_xdg_toplevel_set_tiled(view->surface, false);
    process_move_resize(view, CURSOR_MOVE, 0);
}


static void on_request_resize(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_resize_event *event = data;
    struct view *view = wl_container_of(listener, view, request_resize_listener);
    wlr_xdg_toplevel_set_tiled(view->surface, false);
    process_move_resize(view, CURSOR_RESIZE, event->edges);
}


static void on_request_fullscreen(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_set_fullscreen_event *event = data;
    struct view *view = wl_container_of(listener, view, request_fullscreen_listener);
    fullscreen_xdg_surface(view, event->surface, event->output);
}


static void on_new_xdg_surface(struct wl_listener *listener, void *data) {
    struct wlr_xdg_surface *surface = data;
    if (surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
	return;
    }

    struct view *view = calloc(1, sizeof(struct view));
    view->surface = surface;

    view->map_listener.notify = on_map;
    wl_signal_add(&surface->events.map, &view->map_listener);
    view->unmap_listener.notify = on_unmap;
    wl_signal_add(&surface->events.unmap, &view->unmap_listener);
    view->destroy_listener.notify = on_surface_destroy;
    wl_signal_add(&surface->events.destroy, &view->destroy_listener);

    struct wlr_xdg_toplevel *toplevel = surface->toplevel;
    view->request_move_listener.notify = on_request_move;
    wl_signal_add(&toplevel->events.request_move, &view->request_move_listener);
    view->request_resize_listener.notify = on_request_resize;
    wl_signal_add(&toplevel->events.request_resize, &view->request_resize_listener);
    view->request_fullscreen_listener.notify = on_request_fullscreen;
    wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen_listener);

    if (wimp.scratchpad_waiting) {
	pid_t pid;
	wl_client_get_credentials(surface->client->client, &pid, NULL, NULL);
	struct scratchpad *scratchpad;
	wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	    if (scratchpad->pid == pid) {
		scratchpad->view = view;
		view->is_scratchpad = true;
		view->x = 50;
		view->y = 50;
		map_view(view);
		return;
	    }
	}
    }

    view->is_scratchpad = false;
    view->x = wimp.current_desk->border_width;
    view->y = wimp.current_desk->border_width;
    wl_list_insert(&wimp.current_desk->views, &view->link);
}


void set_up_shell() {
    wimp.shell = wlr_xdg_shell_create(wimp.display);
    wimp.new_xdg_surface_listener.notify = on_new_xdg_surface;
    wl_signal_add(&wimp.shell->events.new_surface, &wimp.new_xdg_surface_listener);
}
