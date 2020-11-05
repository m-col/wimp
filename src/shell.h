#pragma once

#include "types.h"


void on_map(struct wl_listener *listener, void *data);
void on_unmap(struct wl_listener *listener, void *data);
void on_surface_destroy(struct wl_listener *listener, void *data);
void on_request_move(struct wl_listener *listener, void *data);
void on_request_resize(struct wl_listener *listener, void *data);
void on_new_xdg_surface(struct wl_listener *listener, void *data);
void set_up_shell(struct server *server);
