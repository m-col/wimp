#ifndef DESKWM_OUTPUT_H
#define DESKWM_OUTPUT_H

#include "types.h"

void set_up_outputs();
void damage_box(struct wlr_box *geo, bool add_borders);
void damage_by_view(struct view *view, bool with_borders);
void damage_all_outputs();

#endif
