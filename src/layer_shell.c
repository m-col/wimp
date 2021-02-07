#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_damage.h>

#include "layer_shell.h"


static void layer(struct output *output) {
    struct layer_view *lview;
    struct wlr_layer_surface_v1 *surface;
    struct wlr_layer_surface_v1_state *state;
    int width, height;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);

    for (int i = 0; i < 4; i++) {
	if (wl_list_empty(&output->layer_views[i])) {
	    continue;
	}

	wl_list_for_each(lview, &output->layer_views[i], link) {
	    surface = lview->surface;
	    state = &surface->current;
	    struct wlr_box box = {
		.width = state->desired_width,
		.height = state->desired_height
	    };
	    // Horizontal axis
	    const uint32_t both_horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	    if ((state->anchor & both_horiz) && box.width == 0) {
		box.x = state->margin.left;
		box.width = width - state->margin.left - state->margin.right;
	    } else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
		box.x = state->margin.left;
	    } else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
		box.x = width - box.width - state->margin.right;
	    } else {
		box.x = width / 2 - box.width / 2;
	    }
	    // Vertical axis
	    const uint32_t both_vert = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
		    | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	    if ((state->anchor & both_vert) && box.height == 0) {
		box.y = state->margin.top;
		box.height = height - state->margin.top - state->margin.bottom;
	    } else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
		box.y = state->margin.top;
	    } else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
		box.y = height - box.height - state->margin.bottom;
	    } else {
		box.y = height / 2 - box.height / 2;
	    }
	    if (box.width < 0 || box.height < 0) {
		wlr_layer_surface_v1_close(surface);
		continue;
	    }
	    lview->geo = box;
	    wlr_layer_surface_v1_configure(surface, box.width, box.height);
	}
    }

    // Find topmost keyboard interactive layer, if such a layer exists
    uint32_t layers_above_shell[] = {
	ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
	ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    };
    size_t nlayers = sizeof(layers_above_shell) / sizeof(layers_above_shell[0]);
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(wimp.seat);
    for (size_t i = 0; i < nlayers; ++i) {
	wl_list_for_each_reverse(lview, &output->layer_views[layers_above_shell[i]], link) {
	    if (lview->surface->current.keyboard_interactive && lview->surface->mapped) {
		wlr_seat_keyboard_notify_enter(
		    wimp.seat, lview->surface->surface, keyboard->keycodes,
		    keyboard->num_keycodes, &keyboard->modifiers
		);
		wimp.focussed_layer_view = lview;
		return;
	    }
	}
    }

    wlr_output_damage_add_whole(output->wlr_output_damage);
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

    if (wimp.focussed_layer_view == lview && !wl_list_empty(&wimp.current_desk->views)) {
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(wimp.seat);
	struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
	wlr_seat_keyboard_notify_enter(
	    wimp.seat, view->surface->surface, keyboard->keycodes,
	    keyboard->num_keycodes, &keyboard->modifiers
	);
	wimp.focussed_layer_view = NULL;
    }
}


static void on_map(struct wl_listener *listener, void *data) {
    struct layer_view *lview = wl_container_of(listener, lview, map_listener);
    lview->surface->mapped = true;
    layer(lview->output);
    wlr_surface_send_enter(lview->surface->surface, lview->surface->output);
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

    // Temporarily set the layer's current state to client_pending
    // So that we can easily arrange it
    struct wlr_layer_surface_v1_state old_state = surface->current;
    surface->current = surface->client_pending;
    layer(lview->output);
    surface->current = old_state;
}


void set_up_layer_shell() {
    wimp.layer_shell = wlr_layer_shell_v1_create(wimp.display);
    wimp.layer_shell_surface_listener.notify = on_new_surface;
    wl_signal_add(&wimp.layer_shell->events.new_surface, &wimp.layer_shell_surface_listener);
}
