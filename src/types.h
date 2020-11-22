#ifndef DESKWM_TYPES_H
#define DESKWM_TYPES_H

#include <cairo/cairo.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

enum cursor_mode {
    CURSOR_PASSTHROUGH,
    CURSOR_MOD,
    CURSOR_MOVE,
    CURSOR_RESIZE,
};

struct server;

typedef void (*action)(struct server *server, void *data);

struct server {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output_listener;

    struct wlr_xdg_shell *shell;
    struct wl_listener new_xdg_surface_listener;
    struct wl_list desks;
    int desk_count;
    struct desk *current_desk;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_manager;
    struct wl_listener cursor_motion_listener;
    struct wl_listener cursor_motion_absolute_listener;
    struct wl_listener cursor_button_listener;
    struct wl_listener cursor_axis_listener;
    struct wl_listener cursor_frame_listener;

    struct wlr_seat *seat;
    struct wl_listener new_input_listener;
    struct wl_listener request_cursor_listener;
    struct wl_listener request_set_selection_listener;
    struct wl_list keyboards;
    enum wlr_keyboard_modifier mod;
    struct wl_list key_bindings;
    struct wl_list mouse_bindings;
    action on_mouse_motion;
    action on_mouse_scroll;
    bool reverse_scrolling;
    enum cursor_mode cursor_mode;
    struct view *grabbed_view;
    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;
    double zoom_min, zoom_max;
};

struct view {
    struct wl_list link;
    struct server *server;
    struct wlr_xdg_surface *surface;
    struct wl_listener map_listener;
    struct wl_listener unmap_listener;
    struct wl_listener destroy_listener;
    struct wl_listener request_move_listener;
    struct wl_listener request_resize_listener;
    double x, y;
};

struct keyboard {
    struct wl_list link;
    struct server *server;
    struct wlr_input_device *device;
    struct wl_listener modifier_listener;
    struct wl_listener key_listener;
};

struct wallpaper {
    struct wlr_texture *texture;
    int width, height;
};

struct desk {
    struct wl_list link;
    struct server *server;
    struct wl_list views;
    float background[4];
    struct wallpaper *wallpaper;
    double panned_x, panned_y;
    int index;
    double zoom;
};

struct binding {
    struct wl_list link;
    enum wlr_keyboard_modifier mods;
    uint32_t key;
    action action;
    void *data;
};

struct motion {
    double dx;
    double dy;
};

enum mouse_keys {
    MOTION,
    SCROLL,
};

#endif
