#include "output.h"
#include "types.h"


void on_map(struct wl_listener *listener, void *data) {
    return;
}


void on_unmap(struct wl_listener *listener, void *data) {
    return;
}


void on_surface_destroy(struct wl_listener *listener, void *data) {
    return;
}


void on_request_move(struct wl_listener *listener, void *data) {
    return;
}


void on_request_resize(struct wl_listener *listener, void *data) {
    return;
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

	view->map.notify = on_map;
	wl_signal_add(&surface->events.map, &view->map);
	view->unmap.notify = on_unmap;
	wl_signal_add(&surface->events.unmap, &view->unmap);
	view->destroy.notify = on_surface_destroy;
	wl_signal_add(&surface->events.destroy, &view->destroy);

	struct wlr_xdg_toplevel *toplevel = surface->toplevel;
	view->request_move.notify = on_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = on_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

	wl_list_insert(&server->views, &view->link);
}


void set_up_shell(struct server *server) {
    wl_list_init(&server->views);
    server->shell = wlr_xdg_shell_create(server->display);
    server->new_xdg_surface_listener.notify = on_new_xdg_surface;
    wl_signal_add(&server->shell->events.new_surface, &server->new_xdg_surface_listener);
};
