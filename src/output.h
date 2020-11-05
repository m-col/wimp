#pragma once
#include <wlr/types/wlr_output_layout.h>

#include "types.h"


struct output {
    struct wl_list link;
    struct server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame_listener;
};


void on_frame(struct wl_listener *listener, void *data);
void on_new_output(struct wl_listener *listener, void *data);
void set_up_outputs(struct server *server);
