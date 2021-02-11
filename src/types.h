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
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#define is_readable(p) (access(p, R_OK) != -1)
#define is_executable(p) (access(p, R_OK | X_OK) != -1)

enum cursor_mode {
    CURSOR_PASSTHROUGH,
    CURSOR_MOD,
    CURSOR_MOVE,
    CURSOR_RESIZE,
};

typedef void (*action)(void *data);

struct mark_indicator {
    float colour[4];
    struct wlr_box box;
};

struct wimp {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;

    char *config_directory;
    char *config_file;
    char *auto_start;

    struct wlr_output_layout *output_layout;
    struct wl_listener output_layout_change_listener;
    struct wl_list outputs;
    struct wl_listener new_output_listener;
    struct wlr_output_manager_v1 *output_manager;
    struct wl_listener output_manager_apply_listener;
    struct wl_listener output_manager_test_listener;

    struct wlr_xdg_shell *shell;
    struct wl_listener new_xdg_surface_listener;
    struct wl_list desks;
    int desk_count;
    struct desk *current_desk;
    struct wl_list marks;
    bool mark_waiting;
    struct mark_indicator mark_indicator;

    struct wlr_xdg_decoration_manager_v1 *decoration_manager;
    struct wl_listener decoration_listener;
    struct wlr_server_decoration_manager *server_decoration_manager;

    struct wlr_layer_shell_v1 *layer_shell;
    struct wl_listener layer_shell_surface_listener;
    struct layer_view *focussed_layer_view;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_manager;
    struct wl_listener cursor_motion_listener;
    struct wl_listener cursor_motion_absolute_listener;
    struct wl_listener cursor_button_listener;
    struct wl_listener cursor_axis_listener;
    struct wl_listener cursor_frame_listener;
    struct wlr_pointer_gestures_v1 *pointer_gestures;
    struct wl_listener pinch_begin_listener;
    struct wl_listener pinch_update_listener;
    struct wl_listener pinch_end_listener;
    struct wl_listener swipe_begin_listener;
    struct wl_listener swipe_update_listener;
    struct wl_listener swipe_end_listener;

    struct wlr_seat *seat;
    struct wl_listener new_input_listener;
    struct wl_listener request_cursor_listener;
    struct wl_listener request_set_selection_listener;
    struct wl_list keyboards;
    enum wlr_keyboard_modifier mod;
    struct wl_list key_bindings;
    struct wl_list mouse_bindings;

    struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;
    struct wl_listener new_virtual_keyboard_listener;

    action on_mouse_motion;
    action on_mouse_scroll;
    action on_drag1;
    action on_drag2;
    action on_drag3;
    action on_pinch;
    action on_pinch_begin;

    bool reverse_scrolling;
    enum cursor_mode cursor_mode;
    struct view *grabbed_view;
    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;
    double zoom_min, zoom_max;
    bool vt_switching;
    bool auto_focus;

    struct wl_list scratchpads;
    pid_t scratchpad_waiting;
};

extern struct wimp wimp;

struct view {
    struct wl_list link;
    struct wlr_xdg_surface *surface;
    struct wl_listener map_listener;
    struct wl_listener unmap_listener;
    struct wl_listener commit_listener;
    struct wl_listener destroy_listener;
    struct wl_listener request_move_listener;
    struct wl_listener request_resize_listener;
    struct wl_listener request_fullscreen_listener;
    double x, y;
    int width, height;
    bool is_scratchpad;
};

struct output {
    struct wl_list link;
    struct wl_list layer_views[4];
    struct wlr_output *wlr_output;
    struct wlr_output_damage *wlr_output_damage;
    struct wl_listener frame_listener;
    struct wl_listener destroy_listener;
};

struct layer_view {
    struct wl_list link;
    struct wlr_layer_surface_v1 *surface;
    struct wl_listener map_listener;
    struct wl_listener unmap_listener;
    struct wl_listener commit_listener;
    struct wl_listener destroy_listener;
    struct output *output;
    struct wlr_box geo;
};

struct keyboard {
    struct wl_list link;
    struct wlr_input_device *device;
    struct wl_listener modifier_listener;
    struct wl_listener key_listener;
    struct wl_listener destroy_listener;
};

struct wallpaper {
    struct wlr_texture *texture;
    int width, height;
};

struct desk {
    struct wl_list link;
    struct wl_list views;
    float background[4];
    float border_normal[4];
    float border_focus[4];
    int border_width;
    struct wallpaper *wallpaper;
    double panned_x, panned_y;
    int index;
    double zoom;
    struct wlr_xdg_surface *fullscreened;
    struct wlr_box fullscreened_saved_geo;
};

struct binding {
    struct wl_list link;
    enum wlr_keyboard_modifier mods;
    uint32_t key;
    action action;
    action begin;
    void *data;
};

struct motion {
    double dx;
    double dy;
    bool is_percentage;
};

enum mouse_keys {
    MOTION = 1,
    SCROLL = 2,
    DRAG1 = 3,
    DRAG2 = 4,
    DRAG3 = 5,
    PINCH = 6,
};

struct mark {
    struct wl_list link;
    uint32_t key;
    struct desk *desk;
    double zoom, x, y;
};

enum direction {
    NONE = 0,
    UP = 1 << 0,
    RIGHT = 1 << 1,
    DOWN = 1 << 2,
    LEFT = 1 << 3,
};

struct scratchpad {
    struct wl_list link;
    char *command;
    int id;
    pid_t pid;
    struct view *view;
    bool is_mapped;
    struct wlr_box geo; // negative values represent a percentage of the output's geometry
};

#endif
