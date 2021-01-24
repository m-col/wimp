#ifndef DESKWM_INPUT_H
#define DESKWM_INPUT_H

#include "types.h"

struct view *view_at(
    double lx, double ly, struct wlr_surface **surface, double *sx, double *sy
);

void set_up_cursor();

#endif
