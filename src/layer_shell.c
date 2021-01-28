#include <wlr/types/wlr_layer_shell_v1.h>

#include "layer_shell.h"


static void layer(struct output *output) {
    struct layer_view *lview;
    struct wlr_layer_surface_v1 *surface;
    struct wlr_layer_surface_v1_state *state;

    for (int i = 0; i<4; i++) {
	if (wl_list_empty(&output->layer_views[i])) {
	    continue;
	}

	wl_list_for_each(lview, &output->layer_views[i], link) {
	    surface = lview->surface;
	    state = &surface->current;
	    wlr_layer_surface_v1_configure(surface, state->desired_width, state->desired_height);
	}
    }
}


static void on_destroy(struct wl_listener *listener, void *data) {
    struct layer_view *lview = wl_container_of(listener, lview, destroy_listener);
    struct output *output = lview->output;

    wl_list_remove(&lview->destroy_listener.link);
    wl_list_remove(&lview->map_listener.link);
    wl_list_remove(&lview->unmap_listener.link);
    wl_list_remove(&lview->link);
    free(lview);
    layer(output);
}


static void on_unmap(struct wl_listener *listener, void *data) {
    struct layer_view *lview = wl_container_of(listener, lview, unmap_listener);
    lview->surface->mapped = false;
    layer(lview->output);
    wlr_surface_send_enter(lview->surface->surface, lview->surface->output);
}


static void on_map(struct wl_listener *listener, void *data) {
    struct layer_view *lview = wl_container_of(listener, lview, map_listener);
    lview->surface->mapped = true;
    layer(lview->output);
}


static void on_new_surface(struct wl_listener *listener, void *data) {
    struct wlr_layer_surface_v1 *surface = data;

    struct layer_view *lview = calloc(1, sizeof(struct view));
    lview->surface = surface;
    surface->data = lview;

    lview->map_listener.notify = on_map;
    lview->unmap_listener.notify = on_unmap;
    lview->destroy_listener.notify = on_destroy;
    wl_signal_add(&surface->events.map, &lview->map_listener);
    wl_signal_add(&surface->events.unmap, &lview->unmap_listener);
    wl_signal_add(&surface->events.destroy, &lview->destroy_listener);

    if (!surface->output) {
	surface->output = wlr_output_layout_output_at(
	    wimp.output_layout, wimp.cursor->x, wimp.cursor->y
	);
    }
    struct output *output = surface->output->data;
    lview->output = output;
    wl_list_insert(&output->layer_views[surface->client_pending.layer], &lview->link);

    layer(lview->output);
}


void set_up_layer_shell() {
    wimp.layer_shell = wlr_layer_shell_v1_create(wimp.display);
    wimp.layer_shell_surface_listener.notify = on_new_surface;
    wl_signal_add(&wimp.layer_shell->events.new_surface, &wimp.layer_shell_surface_listener);
}
