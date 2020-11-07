#include <wlr/util/edges.h>

#include "config.h"
#include "output.h"
#include "types.h"


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
    wl_list_insert(&server->views, &view->link);
    /* Activate the new surface */
    wlr_xdg_toplevel_set_activated(view->surface, true);
    wlr_seat_keyboard_notify_enter(
	seat, view->surface->surface, keyboard->keycodes,
	keyboard->num_keycodes, &keyboard->modifiers
    );
}


void on_map(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, map_listener);
    view->is_mapped = true;
    focus_view(view, view->surface->surface);
}


void on_unmap(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, unmap_listener);
    view->is_mapped = false;
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

    if (mode == CURSOR_MOVE) {
	server->grab_x = server->cursor->x - view->x;
	server->grab_y = server->cursor->y - view->y;
    } else {
	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(view->surface, &geo_box);

	double border_x = (view->x + geo_box.x) + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
	double border_y = (view->y + geo_box.y) + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
	server->grab_x = server->cursor->x - border_x;
	server->grab_y = server->cursor->y - border_y;

	server->grab_geobox = geo_box;
	server->grab_geobox.x += view->x;
	server->grab_geobox.y += view->y;

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

    wl_list_insert(&server->views, &view->link);
}


static int num_desks = 0;


void add_desk(struct server *server) {
    struct desk *desk = calloc(1, sizeof(struct desk));
    wl_list_insert(server->desks.prev, &desk->link);
    desk->server = server;
    assign_colour("#5D479D", desk->background);
    desk->index = num_desks;
    num_desks++;
}


void remove_desk(struct desk *desk) {
    wl_list_remove(&desk->link);
    free(desk);
}


void next_desk(struct server *server) {
    struct desk *desk;
    if (server->current_desk->index + 1 == num_desks) {
	desk = wl_container_of(server->desks.next, desk, link);
    } else {
	desk = wl_container_of(server->current_desk->link.next, desk, link);
    }
    server->current_desk = desk;
}


void prev_desk(struct server *server) {
    struct desk *desk;
    if (server->current_desk->index == 0) {
	desk = wl_container_of(server->desks.prev, desk, link);
    } else {
	desk = wl_container_of(server->current_desk->link.prev, desk, link);
    }
    server->current_desk = desk;
}


void set_up_shell(struct server *server) {
    wl_list_init(&server->views);
    server->shell = wlr_xdg_shell_create(server->display);
    server->new_xdg_surface_listener.notify = on_new_xdg_surface;
    wl_signal_add(&server->shell->events.new_surface, &server->new_xdg_surface_listener);
}
