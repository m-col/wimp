#ifndef DESKWM_DECORATIONS_H
#define DESKWM_DECORATIONS_H

#include "types.h"

struct decoration {
    struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration;
    struct wl_listener destroy_listener;
    struct wl_listener request_mode_listener;
};

void set_up_decorations(struct server *server);

#endif
