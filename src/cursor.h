#ifndef WIMP_INPUT_H
#define WIMP_INPUT_H

#include "types.h"

void centre_cursor();
void *under_pointer(struct wlr_surface **surface, double *sx, double *sy, bool *is_layer);
void set_up_cursor();

#endif
