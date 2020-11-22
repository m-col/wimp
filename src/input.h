#ifndef DESKWM_INPUT_H
#define DESKWM_INPUT_H

#include "types.h"

enum wlr_keyboard_modifier modifier_by_name(char *mod);
void add_binding(struct server *server, char *data, int line);
void set_up_cursor(struct server *server);
void set_up_keyboard(struct server *server);

#endif
