#ifndef DESKWM_SHELL_H
#define DESKWM_SHELL_H

#include "types.h"

void unmap_view(struct view *view);
void map_view(struct view *view);
void focus_view(struct view *view, struct wlr_surface *surface);
void pan_to_view(struct view *view);
void fullscreen_xdg_surface(
    struct view *view, struct wlr_xdg_surface *xdg_surface, struct wlr_output *output
);
void set_up_shell();

#endif
