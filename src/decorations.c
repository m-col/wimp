#include "decorations.h"


struct decoration {
    struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration;
    struct wl_listener destroy_listener;
    struct wl_listener request_mode_listener;
};


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


void set_up_decorations() {
    // xdg_decoration
    wimp.decoration_manager = wlr_xdg_decoration_manager_v1_create(wimp.display);

    wimp.decoration_listener.notify = on_new_decoration;
    wl_signal_add(
        &wimp.decoration_manager->events.new_toplevel_decoration,
        &wimp.decoration_listener
    );

    // server_decoration
    wimp.server_decoration_manager = wlr_server_decoration_manager_create(wimp.display);
    wlr_server_decoration_manager_set_default_mode(
	wimp.server_decoration_manager,
	WLR_SERVER_DECORATION_MANAGER_MODE_SERVER
    );
}
