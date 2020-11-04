#include <wlr/types/wlr_output_layout.h>

#include "deskwm.h"
#include "output.h"


void on_frame(struct wl_listener *listener, void *data) {
    return;
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
