#ifndef DESKWM_CONFIG_H
#define DESKWM_CONFIG_H

#include "types.h"

void assign_colour(char *hex, float dest[4]);
void schedule_auto_start();
void load_config();
void locate_config();

#endif
