#ifndef DESKWM_DESK_H
#define DESKWM_DESK_H

#include "types.h"

void add_desk(struct server *server);
void remove_desk(struct desk *desk);
void next_desk(struct server *server);
void prev_desk(struct server *server);
void reset_pan(struct desk *desk);
void save_pan(struct desk *desk);

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
};

#endif
