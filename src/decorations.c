#include "decorations.h"


static void on_destroy(struct wl_listener *listener, void *data) {
    struct decoration *deco = wl_container_of(listener, deco, destroy_listener);
    wl_list_remove(&deco->destroy_listener.link);
    wl_list_remove(&deco->request_mode_listener.link);
    free(deco);
}


static void on_request_mode(struct wl_listener *listener, void *data) {
    struct decoration *deco = wl_container_of(listener, deco, request_mode_listener);
    wlr_xdg_toplevel_decoration_v1_set_mode(
	deco->wlr_xdg_decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
    );
}


static void on_new_decoration(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;

    struct decoration *deco = calloc(1, sizeof(struct decoration));
    if (deco == NULL) {
	return;
    }

    deco->wlr_xdg_decoration = wlr_deco;

    wl_signal_add(&wlr_deco->events.destroy, &deco->destroy_listener);
    deco->destroy_listener.notify = on_destroy;

    wl_signal_add(&wlr_deco->events.request_mode, &deco->request_mode_listener);
    deco->request_mode_listener.notify = on_request_mode;

    wlr_xdg_toplevel_decoration_v1_set_mode(
	wlr_deco, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
    );
}


void set_up_decorations(struct server *server) {
    server->decoration_manager = wlr_xdg_decoration_manager_v1_create(server->display);

    server->decoration_listener.notify = on_new_decoration;
    wl_signal_add(
        &server->decoration_manager->events.new_toplevel_decoration,
        &server->decoration_listener
    );
}
