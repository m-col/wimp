#ifndef DESKWM_SHELL_H
#define DESKWM_SHELL_H

#include "types.h"


void focus_view(struct view *view, struct wlr_surface *surface);
void on_map(struct wl_listener *listener, void *data);
void on_unmap(struct wl_listener *listener, void *data);
void on_surface_destroy(struct wl_listener *listener, void *data);
void process_move_resize(struct view *view, enum cursor_mode mode, uint32_t edges);
void on_request_move(struct wl_listener *listener, void *data);
void on_request_resize(struct wl_listener *listener, void *data);
void on_new_xdg_surface(struct wl_listener *listener, void *data);
void set_up_shell(struct server *server);

#endif
