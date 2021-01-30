#ifndef DESKWM_DESK_H
#define DESKWM_DESK_H

#include "types.h"

void configure_desks(int wanted);
void set_desk(struct desk *desk);
void view_to_desk(struct view *view, int index);

#endif
