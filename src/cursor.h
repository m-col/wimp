#ifndef DESKWM_INPUT_H
#define DESKWM_INPUT_H

#include "types.h"

void *under_pointer(struct wlr_surface **surface, double *sx, double *sy, bool *is_layer);
void set_up_cursor();

#endif
