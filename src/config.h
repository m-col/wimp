#ifndef WIMP_CONFIG_H
#define WIMP_CONFIG_H

#include "types.h"

bool wlr_box_from_str(char* str, struct wlr_box *box);
void assign_colour(char *hex, float dest[4]);
void set_configurable(char *message, char *response);
void schedule_startup();

#endif
