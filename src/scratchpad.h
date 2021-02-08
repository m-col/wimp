#ifndef DESKWM_SCRATCHPAD_H
#define DESKWM_SCRATCHPAD_H

#include "types.h"

struct scratchpad *scratchpad_from_view(struct view *view);
struct scratchpad *scratchpad_from_id(int id);
void scratchpad_apply_geo(struct scratchpad *scratchpad);
bool catch_scratchpad(struct view *view);
void drop_scratchpads();

#endif
