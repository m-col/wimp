#include <stdlib.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "deskwm.h"
#include "output.h"


void on_frame(struct wl_listener *listener, void *data) {
    return;
}


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


int main(int argc, char *argv[]) {
    wlr_log_init(WLR_ERROR, NULL);

    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
	    case 'd':
		wlr_log_init(WLR_DEBUG, NULL);
        }
    }

    // create compositor
    struct server server;
    server.display = wl_display_create();
    server.backend = wlr_backend_autocreate(server.display, NULL);
    server.renderer = wlr_backend_get_renderer(server.backend);
    wlr_renderer_init_wl_display(server.renderer, server.display);
    wlr_compositor_create(server.display, server.renderer);

    // set up outputs
    server.output_layout = wlr_output_layout_create();
    wl_list_init(&server.outputs);
    server.new_output_listener.notify = on_new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output_listener);

    // set up shell
    wl_list_init(&server.views);
    server.shell = wlr_xdg_shell_create(server.display);
    server.new_xdg_surface_listener.notify = on_new_xdg_surface;
    wl_signal_add(&server.shell->events.new_surface, &server.new_xdg_surface_listener);




    // begin session
    wl_display_run(server.display);

    // shutdown
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.display);
    return 0;
}
