#include <wlr/util/edges.h>

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


void on_map(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, map_listener);
    focus_view(view, view->surface->surface);
}

void on_unmap(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, unmap_listener);
    wl_list_remove(&view->link);
    wl_list_insert(view->server->current_desk->views.prev, &view->link);
    struct view *next_view = wl_container_of(view->server->current_desk->views.next, view, link);
    focus_view(next_view, next_view->surface->surface);
}


void on_surface_destroy(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, destroy_listener);
    wl_list_remove(&view->link);
    free(view);
}


void process_move_resize(struct view *view, enum cursor_mode mode, uint32_t edges) {
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


void on_request_move(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, request_move_listener);
    process_move_resize(view, CURSOR_MOVE, 0);
}


void on_request_resize(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_resize_event *event = data;
    struct view *view = wl_container_of(listener, view, request_resize_listener);
    process_move_resize(view, CURSOR_RESIZE, event->edges);
}


void on_new_xdg_surface(struct wl_listener *listener, void *data) {
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

    wl_list_insert(&server->current_desk->views, &view->link);
}


void set_up_shell(struct server *server) {
    server->shell = wlr_xdg_shell_create(server->display);
    server->new_xdg_surface_listener.notify = on_new_xdg_surface;
    wl_signal_add(&server->shell->events.new_surface, &server->new_xdg_surface_listener);
}
