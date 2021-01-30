#ifndef DESKWM_ACTION_H
#define DESKWM_ACTION_H

#include "types.h"

void shutdown(void *data);
void exec_command(void *data);
void change_vt(void *data);
void close_window(void *data);
void move_window(void *data);
void focus_in_direction(void *data);
void next_desk(void *data);
void prev_desk(void *data);
void pan_desk(void *data);
void reset_zoom(void *data);
void zoom(void *data);
void zoom_scroll(void *data);
void zoom_pinch(void *data);
void zoom_pinch_begin(void *data);
void set_mark(void *data);
void actually_set_mark(const xkb_keysym_t sym);
void go_to_mark(void *data);
void actually_go_to_mark(const xkb_keysym_t sym);
void toggle_fullscreen(void *data);
void halfimize(void *data);
void reload_config(void *data);
void send_to_desk(void *data);

#endif
