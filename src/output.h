#pragma once
#include <wlr/types/wlr_output_layout.h>

#include "deskwm.h"


struct output {
    struct wl_list link;
    struct server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame_listener;
};


void on_new_output(struct wl_listener *listener, void *data);
