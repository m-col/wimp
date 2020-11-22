#ifndef DESKWM_INPUT_H
#define DESKWM_INPUT_H

#include "types.h"
#include "shell.h"

enum wlr_keyboard_modifier modifier_by_name(char *mod);
void add_binding(struct server *server, char *data, int line);
void process_cursor_move(struct server *server, uint32_t time, double zoom);
void process_cursor_resize(struct server *server, uint32_t time, double zoom);
bool view_at(
    struct view *view, double lx, double ly, struct wlr_surface **surface,
    double *sx, double *sy
);
struct view *desktop_view_at(
    struct server *server, double lx, double ly, struct wlr_surface **surface,
    double *sx, double *sy
);
void process_cursor_motion(struct server *server, uint32_t time, double dx, double dy);
void on_cursor_motion(struct wl_listener *listener, void *data);
void on_cursor_motion_absolute(struct wl_listener *listener, void *data);
void on_cursor_button(struct wl_listener *listener, void *data);
void on_cursor_axis(struct wl_listener *listener, void *data);
void on_cursor_frame(struct wl_listener *listener, void *data);
void on_modifier(struct wl_listener *listener, void *data);
void on_key(struct wl_listener *listener, void *data);
void on_new_keyboard(struct server *server, struct wlr_input_device *device);
void on_new_input(struct wl_listener *listener, void *data);
void on_request_cursor(struct wl_listener *listener, void *data);
void on_request_set_selection(struct wl_listener *listener, void *data);
void set_up_cursor(struct server *server);
void set_up_keyboard(struct server *server);

struct key_binding {
    struct wl_list link;
    enum wlr_keyboard_modifier mods;
    uint32_t key;
    action action;
    void *data;
};

#endif
