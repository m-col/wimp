#ifndef DESKWM_CONFIG_H
#define DESKWM_CONFIG_H

#include "types.h"

void assign_colour(char *hex, float dest[4]);
void load_config(struct server *server);
void locate_config(struct server *server);

#endif
