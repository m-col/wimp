#ifndef DESKWM_ACTION_H
#define DESKWM_ACTION_H

#include "types.h"

void shutdown(struct server *server, void *data);
void exec_command(struct server *server, void *data);
void change_vt(struct server *server, void *data);
void close_window(struct server *server, void *data);
void focus_in_direction(struct server *server, void *data);
void next_desk(struct server *server, void *data);
void prev_desk(struct server *server, void *data);
void pan_desk(struct server *server, void *data);
void reset_zoom(struct server *server, void *data);
void zoom(struct server *server, void *data);
void zoom_mouse(struct server *server, void *data);
void set_mark(struct server *server, void *data);
void actually_set_mark(struct server *server, const xkb_keysym_t sym);
void go_to_mark(struct server *server, void *data);
void actually_go_to_mark(struct server *server, const xkb_keysym_t sym);
void toggle_fullscreen(struct server *server, void *data);
void halfimize(struct server *server, void *data);
void reload_config(struct server *server, void *data);

#endif
