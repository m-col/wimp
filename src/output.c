#include <time.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_layout.h>
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

    double x = 0, y = 0;
    struct wlr_output_layout_output *ol;
    wl_list_for_each(ol, &wimp.output_layout->outputs, link) {
	if (ol->output == output) {
	    x = - (double)ol->x;
	    y = - (double)ol->y;
	    break;
	}
    }
    x = (x + rdata->x + sx) * output->scale * rdata->zoom;
    y = (y + rdata->y + sy) * output->scale * rdata->zoom;
    int width = surface->current.width * output->scale * rdata->zoom;
    int height = surface->current.height * output->scale * rdata->zoom;

    if (rdata->bordered == surface) {
	int border_width = wimp.current_desk->border_width;
	struct wlr_box borders = {
	    .x = x - border_width,
	    .y = y - border_width,
	    .width = width + border_width * 2,
	    .height = height + border_width * 2,
	};
	wlr_render_rect(
	    rdata->renderer,
	    &borders,
	    rdata->is_focussed ? wimp.current_desk->border_focus : wimp.current_desk->border_normal,
	    output->transform_matrix
	);
    }

    struct wlr_box box = {
	.x = x,
	.y = y,
	.width = width,
	.height = height,
    };
    float matrix[9];
    enum wl_output_transform transform =
	wlr_output_transform_invert(surface->current.transform);
    wlr_matrix_project_box(matrix, &box, transform, 0,
	output->transform_matrix);
    wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

    wlr_surface_send_frame_done(surface, rdata->when);
}


static void on_frame(struct wl_listener *listener, void *data) {
    struct output *output = wl_container_of(listener, output, frame_listener);
    struct wlr_renderer *renderer = wimp.renderer;
    struct desk *desk = wimp.current_desk;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (!wlr_output_attach_render(output->wlr_output, NULL)) {
	return;
    }

    int width, height;
    double zoom = desk->zoom;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    wlr_renderer_begin(renderer, width, height);
    wlr_renderer_clear(renderer, desk->background);

    struct render_data rdata = {
	.output = output->wlr_output,
	.renderer = renderer,
	.bordered = NULL,
	.when = &now,
	.zoom = zoom,
	.is_focussed = false,
	.x = 0,
	.y = 0,
    };

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

    // paint background and bottom layers
    struct layer_view *lview;
    wl_list_for_each(lview, &output->layer_views, link) {
	if (lview->surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
	    wlr_surface_for_each_surface(
		lview->surface->surface, render_surface, &rdata
	    );
	}
    }
    wl_list_for_each(lview, &output->layer_views, link) {
	if (lview->surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM) {
	    wlr_surface_for_each_surface(
		lview->surface->surface, render_surface, &rdata
	    );
	}
    }

    // paint clients
    struct view *view;
    struct view *focussed = wl_container_of(wimp.current_desk->views.next, focussed, link);
    wl_list_for_each_reverse(view, &desk->views, link) {
	rdata.x = view->x;
	rdata.y = view->y;
	rdata.is_focussed = (view == focussed);
	rdata.bordered = view->surface->surface;
	wlr_xdg_surface_for_each_surface(view->surface, render_surface, &rdata);
    }

    rdata.bordered = NULL;
    rdata.is_focussed = false;
    rdata.x = 0;
    rdata.y = 0;

    // paint top and overlay layers
    wl_list_for_each(lview, &output->layer_views, link) {
	if (lview->surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
	    wlr_surface_for_each_surface(
		lview->surface->surface, render_surface, &rdata
	    );
	}
    }
    wl_list_for_each(lview, &output->layer_views, link) {
	if (lview->surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
	    wlr_surface_for_each_surface(
		lview->surface->surface, render_surface, &rdata
	    );
	}
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

    output->frame_listener.notify = on_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame_listener);
    wl_list_insert(&wimp.outputs, &output->link);
    wlr_output_layout_add_auto(wimp.output_layout, wlr_output);

    wl_list_init(&output->layer_views);
}


void set_up_outputs() {
    wimp.output_layout = wlr_output_layout_create();
    wl_list_init(&wimp.outputs);
    wimp.new_output_listener.notify = on_new_output;
    wl_signal_add(&wimp.backend->events.new_output, &wimp.new_output_listener);

    wlr_xdg_output_manager_v1_create(wimp.display, wimp.output_layout);
}
