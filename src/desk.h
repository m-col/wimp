#ifndef DESKWM_DESK_H
#define DESKWM_DESK_H

#include "types.h"

void add_desk(struct server *server);
void remove_desk(struct desk *desk);
void next_desk(struct server *server);
void prev_desk(struct server *server);
void reset_pan(struct desk *desk);
void save_pan(struct desk *desk);

#endif
