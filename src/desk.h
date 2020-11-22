#ifndef DESKWM_DESK_H
#define DESKWM_DESK_H

#include "types.h"

void add_desk(struct server *server);
void set_desk(struct server *server, struct desk *desk);

#endif
