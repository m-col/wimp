#include <time.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_layout.h>

#include "output.h"
#include "types.h"


void render_surface(
    struct wlr_surface *surface, int sx, int sy, void *data
) {
    struct render_data *rdata = data;
    struct view *view = rdata->view;
    struct wlr_output *output = rdata->output;

    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (texture == NULL) {
	return;
    }

    // get output-relative coordinates
    double ox = 0, oy = 0;
    wlr_output_layout_output_coords(
	view->server->output_layout, output, &ox, &oy
    );
    ox += view->x + sx, oy += view->y + sy;

    // scale for HiDPI
    struct wlr_box box = {
	.x = ox * output->scale,
	.y = oy * output->scale,
	.width = surface->current.width * output->scale,
	.height = surface->current.height * output->scale,
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


void on_frame(struct wl_listener *listener, void *data) {
    struct output *output = wl_container_of(listener, output, frame_listener);
    struct wlr_renderer *renderer = output->server->renderer;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (!wlr_output_attach_render(output->wlr_output, NULL)) {
	return;
    }

    int width, height;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    wlr_renderer_begin(renderer, width, height);

    wlr_renderer_clear(renderer, output->server->background);

    struct view *view;
    wl_list_for_each_reverse(view, &output->server->views, link) {
	if (!view->is_mapped) {
	    continue;
	}
	struct render_data rdata = {
	    .output = output->wlr_output,
	    .view = view,
	    .renderer = renderer,
	    .when = &now,
	};
	wlr_xdg_surface_for_each_surface(
	    view->surface, render_surface, &rdata
	);
    }

    wlr_output_render_software_cursors(output->wlr_output, NULL);  // no-op with HW cursors
    wlr_renderer_end(renderer);
    wlr_output_commit(output->wlr_output);
}


void on_new_output(struct wl_listener *listener, void *data) {
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
