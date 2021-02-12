#include <math.h>
#include <time.h>
#include <pixman.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>

#include "output.h"
#include "types.h"


struct render_data {
    struct wlr_output *output;
    struct wlr_renderer *renderer;
    struct wlr_surface *bordered;
    struct timespec *when;
    double zoom;
    bool is_focussed;
    int x;
    int y;
};


static void render_surface(
    struct wlr_surface *surface, int sx, int sy, void *data
) {
    struct render_data *rdata = data;
    struct wlr_output *output = rdata->output;

    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (texture == NULL) {
	return;
    }

    int x = (rdata->x + sx) * output->scale * rdata->zoom;
    int y = (rdata->y + sy) * output->scale * rdata->zoom;
    int width = surface->current.width * output->scale * rdata->zoom;
    int height = surface->current.height * output->scale * rdata->zoom;

    if (rdata->bordered == surface) {
	int border_width = wimp.current_desk->border_width;
	if (border_width > 0) {
	    int remainder = ceil(border_width * 2 * rdata->zoom);
	    struct wlr_box borders = {
		.x = x - border_width * rdata->zoom,
		.y = y - border_width * rdata->zoom,
		.width = width + remainder,
		.height = height + remainder,
	    };
	    wlr_render_rect(
		rdata->renderer,
		&borders,
		rdata->is_focussed ? wimp.current_desk->border_focus : wimp.current_desk->border_normal,
		output->transform_matrix
	    );
	    if (wimp.mod_on) {
		struct wlr_box corners = {
		    .x = borders.x,
		    .y = borders.y,
		    .width = 24,
		    .height = 24,
		};
		wlr_render_rect(
		    rdata->renderer, &corners, wimp.current_desk->corner_resize, output->transform_matrix
		);
		corners.x = borders.x + borders.width - 24;
		wlr_render_rect(
		    rdata->renderer, &corners, wimp.current_desk->corner_resize, output->transform_matrix
		);
		corners.y = borders.y + borders.height - 24;
		wlr_render_rect(
		    rdata->renderer, &corners, wimp.current_desk->corner_resize, output->transform_matrix
		);
		corners.x = borders.x;
		wlr_render_rect(
		    rdata->renderer, &corners, wimp.current_desk->corner_resize, output->transform_matrix
		);
	    }
	}
    }

    struct wlr_box box = {
	.x = x,
	.y = y,
	.width = width,
	.height = height,
    };

    float matrix[9];
    enum wl_output_transform transform = wlr_output_transform_invert(surface->current.transform);
    wlr_matrix_project_box(matrix, &box, transform, 0, output->transform_matrix);
    wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

    wlr_surface_send_frame_done(surface, rdata->when);
}


static void on_frame(struct wl_listener *listener, void *data) {
    struct output *output = wl_container_of(listener, output, frame_listener);

    bool needs_frame;
    pixman_region32_t damage;
    pixman_region32_init(&damage);

    if (!wlr_output_damage_attach_render(output->wlr_output_damage, &needs_frame, &damage)) {
	goto finish;
    }

    if (!needs_frame) {
	wlr_output_rollback(output->wlr_output);
	goto finish;
    }

    struct wlr_renderer *renderer = wimp.renderer;
    struct desk *desk = wimp.current_desk;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int width, height;
    double zoom = desk->zoom;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    wlr_renderer_begin(renderer, width, height);
    wlr_renderer_clear(renderer, desk->background);

    double ox, oy;
    struct wlr_output_layout_output *ol;
    wl_list_for_each(ol, &wimp.output_layout->outputs, link) {
	if (ol->output == output->wlr_output) {
	    ox = (double)ol->x;
	    oy = (double)ol->y;
	    break;
	}
    }

    // paint wallpaper
    struct wallpaper *wallpaper = desk->wallpaper;
    if (wallpaper != NULL) {
	int wh = wallpaper->height;
	int ww = wallpaper->width;
	float mat[9];
	memcpy(mat, output->wlr_output->transform_matrix, 9 * sizeof(float));
	wlr_matrix_scale(mat, zoom, zoom);
	for (int x = ((int)desk->panned_x % ww) - ww; x < width / zoom; x += ww) {
	    for (int y = ((int)desk->panned_y % wh) - wh; y < height / zoom; y += wh) {
		wlr_render_texture(renderer, wallpaper->texture, mat, x, y, 1.0f);
	    }
	}
    }

    struct render_data rdata = {
	.output = output->wlr_output,
	.renderer = renderer,
	.bordered = NULL,
	.when = &now,
	.zoom = 1,
	.is_focussed = false,
	.x = 0,
	.y = 0,
    };

    // paint background and bottom layers
    struct layer_view *lview;
    wl_list_for_each(lview, &output->layer_views[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], link) {
	wlr_surface_for_each_surface(
	    lview->surface->surface, render_surface, &rdata
	);
    }
    wl_list_for_each(lview, &output->layer_views[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], link) {
	wlr_surface_for_each_surface(
	    lview->surface->surface, render_surface, &rdata
	);
    }

    rdata.zoom = zoom;

    // paint clients
    struct view *view;
    struct wlr_surface *focussed = wimp.seat->keyboard_state.focused_surface;
    int border_width = wimp.current_desk->border_width;
    wl_list_for_each_reverse(view, &desk->views, link) {
	rdata.x = view->x - ox / zoom;
	rdata.y = view->y - oy / zoom;
	if (
		(rdata.x + view->surface->geometry.width + border_width < 0) ||
		(rdata.y + view->surface->geometry.height + border_width < 0) ||
		(rdata.x - border_width > width / zoom) || (rdata.y - border_width > height / zoom)
	) {
	    continue;
	}
	rdata.is_focussed = (view->surface->surface == focussed);
	rdata.bordered = view->surface->surface;
	wlr_xdg_surface_for_each_surface(view->surface, render_surface, &rdata);
    }

    rdata.zoom = 1;

    struct scratchpad *scratchpad;
    wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	if (scratchpad->is_mapped) {
	    view = scratchpad->view;
	    rdata.x = view->x;
	    rdata.y = view->y;
	    rdata.is_focussed = (view->surface->surface == focussed);
	    rdata.bordered = view->surface->surface;
	    wlr_xdg_surface_for_each_surface(view->surface, render_surface, &rdata);
	}
    }

    rdata.bordered = NULL;
    rdata.is_focussed = false;

    // paint top and overlay layers
    wl_list_for_each(lview, &output->layer_views[ZWLR_LAYER_SHELL_V1_LAYER_TOP], link) {
	rdata.x = lview->geo.x;
	rdata.y = lview->geo.y;
	wlr_surface_for_each_surface(
	    lview->surface->surface, render_surface, &rdata
	);
    }
    wl_list_for_each(lview, &output->layer_views[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], link) {
	rdata.x = lview->geo.x;
	rdata.y = lview->geo.y;
	wlr_surface_for_each_surface(
	    lview->surface->surface, render_surface, &rdata
	);
    }

    // paint mark indicator
    if (wimp.mark_waiting) {
	struct wlr_box indicator = wimp.mark_indicator.box;
	indicator.y = height - indicator.height;
	wlr_render_rect(
	    renderer, &indicator, wimp.mark_indicator.colour,
	    output->wlr_output->transform_matrix
	);
    }

    wlr_output_render_software_cursors(output->wlr_output, NULL);  // no-op with HW cursors
    wlr_renderer_end(renderer);
    wlr_output_commit(output->wlr_output);

finish:
    pixman_region32_fini(&damage);
}


void damage_box(struct wlr_box *geo, bool add_borders) {
    struct output *output;

    if (add_borders) {
	struct wlr_box damaged;
	int border_width = wimp.current_desk->border_width;
	memcpy(&damaged, geo, sizeof(struct wlr_box));
	damaged.x -= border_width;
	damaged.y -= border_width;
	damaged.width += border_width * 2;
	damaged.height += border_width * 2;
	wl_list_for_each(output, &wimp.outputs, link) {
	    wlr_output_damage_add_box(output->wlr_output_damage, &damaged);
	}
    } else {
	wl_list_for_each(output, &wimp.outputs, link) {
	    wlr_output_damage_add_box(output->wlr_output_damage, geo);
	}
    }
}


void damage_by_view(struct view *view, bool with_borders) {
    double zoom = wimp.current_desk->zoom;

    struct wlr_box geo = {
	.x = view->x,
	.y = view->y,
	.width = view->width * zoom,
	.height = view->height * zoom,
    };

    damage_box(&geo, with_borders);
}


void damage_by_lview(struct layer_view *lview) {
    damage_box(&lview->geo, false);
}


void damage_all_outputs() {
    struct output *output;
    wl_list_for_each(output, &wimp.outputs, link) {
	wlr_output_damage_add_whole(output->wlr_output_damage);
    }
}


void damage_all_views() {
    struct view *view;
    wl_list_for_each(view, &wimp.current_desk->views, link) {
	damage_by_view(view, true);
    }
}


void damage_mark_indicator() {
    struct output *output;
    struct wlr_box indicator = wimp.mark_indicator.box;
    int width, height;

    wl_list_for_each(output, &wimp.outputs, link) {
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	struct wlr_box geo = {
	    .x = 0,
	    .y = height - indicator.height,
	    .width = indicator.width,
	    .height = indicator.height,
	};
	wlr_output_damage_add_box(output->wlr_output_damage, &geo);
    }
}


static void on_destroy(struct wl_listener *listener, void *data) {
    struct output *output = wl_container_of(listener, output, destroy_listener);

    struct layer_view *lview, *tlview;
    for (int i = 0; i < 4; i++ ) {
        wl_list_for_each_safe(lview, tlview, &output->layer_views[i], link) {
            wl_list_remove(&lview->link);
            wl_list_remove(&lview->map_listener.link);
            wl_list_remove(&lview->unmap_listener.link);
            wl_list_remove(&lview->destroy_listener.link);
            free(lview);
        };
    }

    wl_list_remove(&output->frame_listener.link);
    wl_list_remove(&output->destroy_listener.link);
    wl_list_remove(&output->link);
    free(output);
}


static void on_new_output(struct wl_listener *listener, void *data) {
    struct wlr_output *wlr_output = data;

    if (!wl_list_empty(&wlr_output->modes)) {
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	wlr_output_set_mode(wlr_output, mode);
	wlr_output_enable(wlr_output, true);
	if (!wlr_output_commit(wlr_output)) {
	    return;
	}
    }

    struct output *output = calloc(1, sizeof(struct output));
    output->wlr_output = wlr_output;
    wlr_output->data = output;
    output->wlr_output_damage = wlr_output_damage_create(wlr_output);

    output->frame_listener.notify = on_frame;
    output->destroy_listener.notify = on_destroy;
    wl_signal_add(&output->wlr_output_damage->events.frame, &output->frame_listener);
    wl_signal_add(&wlr_output->events.destroy, &output->destroy_listener);

    wl_list_insert(&wimp.outputs, &output->link);
    wlr_output_layout_add_auto(wimp.output_layout, wlr_output);

    wl_list_init(&output->layer_views[0]);
    wl_list_init(&output->layer_views[1]);
    wl_list_init(&output->layer_views[2]);
    wl_list_init(&output->layer_views[3]);
}


static void output_manager_reconfigure(
    struct wlr_output_configuration_v1 *config, bool commit
) {
    struct wlr_output_configuration_head_v1 *config_head;
    bool ok = 1;

    wl_list_for_each(config_head, &config->heads, link) {
        struct wlr_output *wlr_output = config_head->state.output;

        wlr_output_enable(wlr_output, config_head->state.enabled);
        if (config_head->state.enabled) {
            if (config_head->state.mode) {
                wlr_output_set_mode(wlr_output, config_head->state.mode);
	    } else {
                wlr_output_set_custom_mode(
		    wlr_output,
		    config_head->state.custom_mode.width,
		    config_head->state.custom_mode.height,
		    config_head->state.custom_mode.refresh
		);
	    }

            wlr_output_layout_move(
		wimp.output_layout, wlr_output, config_head->state.x, config_head->state.y
	    );
            wlr_output_set_transform(wlr_output, config_head->state.transform);
            wlr_output_set_scale(wlr_output, config_head->state.scale);
        }

        ok = wlr_output_test(wlr_output);
        if (!ok) {
            break;
	}
    }

    wl_list_for_each(config_head, &config->heads, link) {
        if (ok && commit) {
            wlr_output_commit(config_head->state.output);
	} else {
            wlr_output_rollback(config_head->state.output);
	}
    }

    if (ok) {
        wlr_output_configuration_v1_send_succeeded(config);
    } else {
        wlr_output_configuration_v1_send_failed(config);
    }
    wlr_output_configuration_v1_destroy(config);
}


static void on_output_manager_apply(struct wl_listener *listener, void *data) {
    output_manager_reconfigure(data, true);
}


static void on_output_manager_test(struct wl_listener *listener, void *data) {
    output_manager_reconfigure(data, false);
}


static void on_output_layout_change(struct wl_listener *listener, void *data) {
    struct wlr_output_configuration_v1 *config = wlr_output_configuration_v1_create();

    struct output *output;
    wl_list_for_each(output, &wimp.outputs, link) {
	struct wlr_output_configuration_head_v1 *config_head =
	    wlr_output_configuration_head_v1_create(config, output->wlr_output);

	struct wlr_box *box = wlr_output_layout_get_box(wimp.output_layout, output->wlr_output);
	config_head->state.x = box->x;
	config_head->state.y = box->y;
	config_head->state.enabled = output->wlr_output->enabled;
	config_head->state.mode = output->wlr_output->current_mode;
    }

    wlr_output_manager_v1_set_configuration(wimp.output_manager, config);
}


void set_up_outputs() {
    wimp.output_layout = wlr_output_layout_create();
    wimp.output_layout_change_listener.notify = on_output_layout_change;
    wl_signal_add(&wimp.output_layout->events.change, &wimp.output_layout_change_listener);

    wl_list_init(&wimp.outputs);
    wimp.new_output_listener.notify = on_new_output;
    wl_signal_add(&wimp.backend->events.new_output, &wimp.new_output_listener);

    wlr_screencopy_manager_v1_create(wimp.display);
    wlr_xdg_output_manager_v1_create(wimp.display, wimp.output_layout);

    wimp.output_manager = wlr_output_manager_v1_create(wimp.display);
    wimp.output_manager_apply_listener.notify = on_output_manager_apply;
    wimp.output_manager_test_listener.notify = on_output_manager_test;
    wl_signal_add(&wimp.output_manager->events.apply, &wimp.output_manager_apply_listener);
    wl_signal_add(&wimp.output_manager->events.test, &wimp.output_manager_test_listener);

    wlr_gamma_control_manager_v1_create(wimp.display);
}
