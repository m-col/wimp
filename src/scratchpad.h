#ifndef DESKWM_SCRATCHPAD_H
#define DESKWM_SCRATCHPAD_H

#include "types.h"

struct scratchpad *scratchpad_from_view(struct view *view);
void scratchpad_apply_geo(struct scratchpad *scratchpad);
bool catch_scratchpad(struct view *view);

#endif
