#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>

#include "action.h"
#include "types.h"


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
    struct server *server = view->server;
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
    if (prev_surface == surface) {
	return;
    }
    if (prev_surface) {
	struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
	    seat->keyboard_state.focused_surface
	);
	wlr_xdg_toplevel_set_activated(previous, false);
    }
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    /* Move the view to the front */
    wl_list_remove(&view->link);
    wl_list_insert(&server->current_desk->views, &view->link);
    /* Activate the new surface */
    wlr_xdg_toplevel_set_activated(view->surface, true);
    wlr_seat_keyboard_notify_enter(
	seat, view->surface->surface, keyboard->keycodes,
	keyboard->num_keycodes, &keyboard->modifiers
    );
}


void pan_to_view(struct view *view) {
    struct server *server = view->server;
    int border_width = server->current_desk->border_width;
    double x = view->x - border_width;
    double y = view->y - border_width;
    double width = view->surface->geometry.width + border_width * 2;
    double height = view->surface->geometry.height + border_width * 2;
    struct wlr_box *extents = wlr_output_layout_get_box(server->output_layout, NULL);
    struct motion motion = {
	.dx = 0,
	.dy = 0,
	.is_percentage = false,
    };

    if (x < 0) {
	motion.dx = x;
    } else if (x + width > extents->width) {
	motion.dx = x + width - extents->width;
    }
    if (y < 0) {
	motion.dy = y;
    } else if (y + height > extents->height) {
	motion.dy = y + height - extents->height;
    }
    pan_desk(server, &motion);
}


void fullscreen_xdg_surface(
    struct view *view, struct wlr_xdg_surface *xdg_surface, struct wlr_output *output
) {
    /* output can be NULL, in which case it is calculated using the surface's position */
    struct server *server = view->server;
    struct wlr_box *saved_geo = &server->current_desk->fullscreened_saved_geo;

    struct wlr_xdg_surface *prev_surface = server->current_desk->fullscreened;
    if (prev_surface) {
	wlr_xdg_toplevel_set_fullscreen(prev_surface, false);
	wlr_xdg_toplevel_set_size(prev_surface, saved_geo->width, saved_geo->height);
	view->x = saved_geo->x;
	view->y = saved_geo->y;
    }

    if (prev_surface == xdg_surface) {
	server->current_desk->fullscreened = NULL;
	return;
    }

    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
	return;

    if (!output) {
	double x = view->x + xdg_surface->geometry.width / 2;
	double y = view->y + xdg_surface->geometry.height / 2;
	double lx, ly;
	wlr_output_layout_closest_point(server->output_layout, NULL, x, y, &lx, &ly);
	output = wlr_output_layout_output_at(server->output_layout, lx, ly);
    }

    server->current_desk->fullscreened = xdg_surface;
    saved_geo->x = view->x;
    saved_geo->y = view->y;
    view->x = view->y = 0;
    saved_geo->width = xdg_surface->geometry.width;
    saved_geo->height = xdg_surface->geometry.height;
    wlr_xdg_toplevel_set_fullscreen(xdg_surface, true);
    double zoom = server->current_desk->zoom;
    wlr_xdg_toplevel_set_size(xdg_surface, output->width / zoom, output->height / zoom);
}


static void on_map(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, map_listener);
    if (view->server->can_steal_focus)
	focus_view(view, view->surface->surface);
}

static void on_unmap(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, unmap_listener);
    if (view->server->can_steal_focus) {
	wl_list_remove(&view->link);
	wl_list_insert(view->server->current_desk->views.prev, &view->link);
	struct view *next_view = wl_container_of(view->server->current_desk->views.next, view, link);
	focus_view(next_view, next_view->surface->surface);
    }
}


static void on_surface_destroy(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, destroy_listener);
    wl_list_remove(&view->link);
    free(view);
}


static void process_move_resize(struct view *view, enum cursor_mode mode, uint32_t edges) {
    struct server *server = view->server;
    struct wlr_surface *focused_surface =
	server->seat->pointer_state.focused_surface;

    if (view->surface->surface != focused_surface) {
	    return;
    }
    server->grabbed_view = view;
    server->cursor_mode = mode;

    double zoom = server->current_desk->zoom;
    server->grab_x = server->cursor->x - view->x * zoom;
    server->grab_y = server->cursor->y - view->y * zoom;

    if (mode == CURSOR_RESIZE) {
	wlr_xdg_surface_get_geometry(view->surface, &server->grab_geobox);
	server->grab_geobox.x = view->x;
	server->grab_geobox.y = view->y;
	server->resize_edges = edges;
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
    struct server *server = wl_container_of(listener, server, new_xdg_surface_listener);
    struct wlr_xdg_surface *surface = data;
    if (surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
	return;
    }

    struct view *view = calloc(1, sizeof(struct view));
    view->server = server;
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

    wl_list_insert(&server->current_desk->views, &view->link);
}


void set_up_shell(struct server *server) {
    server->shell = wlr_xdg_shell_create(server->display);
    server->new_xdg_surface_listener.notify = on_new_xdg_surface;
    wl_signal_add(&server->shell->events.new_surface, &server->new_xdg_surface_listener);
}
