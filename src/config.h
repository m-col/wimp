#ifndef WIMP_CONFIG_H
#define WIMP_CONFIG_H

#include "types.h"

void assign_colour(char *hex, float dest[4]);
void schedule_auto_start();
void parse_message(char *buffer, ssize_t len);
void load_config();
void locate_config();

#endif
