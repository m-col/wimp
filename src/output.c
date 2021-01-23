#include <time.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_layout.h>

#include "output.h"
#include "types.h"


struct output {
    struct wl_list link;
    struct server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame_listener;
};


struct render_data {
    struct wlr_output *output;
    struct wlr_renderer *renderer;
    struct view *view;
    struct timespec *when;
    double zoom;
};


static void render_surface(
    struct wlr_surface *surface, int sx, int sy, void *data
) {
    struct render_data *rdata = data;
    struct view *view = rdata->view;
    struct wlr_output *output = rdata->output;
    struct server *server = view->server;

    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (texture == NULL) {
	return;
    }

    double x = 0, y = 0;
    struct wlr_output_layout_output *ol;
    wl_list_for_each(ol, &server->output_layout->outputs, link) {
	if (ol->output == output) {
	    x = - (double)ol->x;
	    y = - (double)ol->y;
	    break;
	}
    }
    x += view->x + sx;
    y += view->y + sy;

    // scale for HiDPI
    struct wlr_box box = {
	.x = x * output->scale * rdata->zoom,
	.y = y * output->scale * rdata->zoom,
	.width = surface->current.width * output->scale * rdata->zoom,
	.height = surface->current.height * output->scale * rdata->zoom,
    };

    // this is where any rendering magic might happen
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
    struct server *server = output->server;
    struct wlr_renderer *renderer = server->renderer;
    struct desk *desk = server->current_desk;

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

    // paint clients
    struct view *view;
    wl_list_for_each_reverse(view, &desk->views, link) {
	struct render_data rdata = {
	    .output = output->wlr_output,
	    .view = view,
	    .renderer = renderer,
	    .when = &now,
	    .zoom = zoom,
	};
	wlr_xdg_surface_for_each_surface(
	    view->surface, render_surface, &rdata
	);
    }

    // paint mark indicator
    if (server->mark_waiting) {
	struct wlr_box indicator = server->mark_indicator.box;
	indicator.y = height - indicator.height;
	wlr_render_rect(
	    renderer, &indicator, server->mark_indicator.colour,
	    output->wlr_output->transform_matrix
	);
    }

    wlr_output_render_software_cursors(output->wlr_output, NULL);  // no-op with HW cursors
    wlr_renderer_end(renderer);
    wlr_output_commit(output->wlr_output);
}


static void on_new_output(struct wl_listener *listener, void *data) {
    struct server *server = wl_container_of(listener, server, new_output_listener);
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
    output->server = server;
    output->frame_listener.notify = on_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame_listener);
    wl_list_insert(&server->outputs, &output->link);
    wlr_output_layout_add_auto(server->output_layout, wlr_output);
}


void set_up_outputs(struct server *server) {
    server->output_layout = wlr_output_layout_create();
    wl_list_init(&server->outputs);
    server->new_output_listener.notify = on_new_output;
    wl_signal_add(&server->backend->events.new_output, &server->new_output_listener);
}
