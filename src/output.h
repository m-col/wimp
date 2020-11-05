#pragma once
#include <wlr/types/wlr_output_layout.h>

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
};


void render_surface(
    struct wlr_surface *surface, int sx, int sy, void *data
);
void on_frame(struct wl_listener *listener, void *data);
void on_new_output(struct wl_listener *listener, void *data);
void set_up_outputs(struct server *server);
