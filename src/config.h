#ifndef WIMP_CONFIG_H
#define WIMP_CONFIG_H

#include "types.h"

void assign_colour(char *hex, float dest[4]);
void parse_message(char *buffer);
void schedule_startup();
void set_up_defaults();

#endif
