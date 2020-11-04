#pragma once

#include <stdlib.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>


struct server {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output_listener;

    struct wlr_xdg_shell *shell;
    struct wl_listener new_xdg_surface_listener;
    struct wl_list views;
};


struct view {
    struct wl_list link;
    struct server *server;
    struct wlr_xdg_surface *surface;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    bool is_mapped;
    int x, y;
};


void create_serve();
