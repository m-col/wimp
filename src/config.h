#ifndef DESKWM_CONFIG_H
#define DESKWM_CONFIG_H

#include "types.h"

void assign_colour(char *hex, float dest[4]);
void load_defaults(struct server *server);
void load_config(struct server *server, FILE *stream);
void locate_config(struct server *server);

#endif
