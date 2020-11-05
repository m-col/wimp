#pragma once

#include "types.h"


void on_cursor_motion(struct wl_listener *listener, void *data);
void on_cursor_motion_absolute(struct wl_listener *listener, void *data);
void on_cursor_button(struct wl_listener *listener, void *data);
void on_cursor_axis(struct wl_listener *listener, void *data);
void on_cursor_frame(struct wl_listener *listener, void *data);
void on_new_input(struct wl_listener *listener, void *data);
void on_request_cursor(struct wl_listener *listener, void *data);
void on_request_set_selection(struct wl_listener *listener, void *data);
void set_up_cursor(struct server *server);
void set_up_keyboard(struct server *server);
