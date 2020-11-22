#ifndef DESKWM_ACTION_H
#define DESKWM_ACTION_H

#include "types.h"

void shutdown(struct server *server, void *data);
void exec_command(struct server *server, void *data);
void close_current_window(struct server *server, void *data);
void next_desk(struct server *server, void *data);
void prev_desk(struct server *server, void *data);
void pan_desk(struct server *server, void *data);
void reset_pan(struct server *server, void *data);
void save_pan(struct server *server, void *data);
void zoom_desk(struct server *server, void *data);

#endif
