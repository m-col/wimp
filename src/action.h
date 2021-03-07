#ifndef WIMP_ACTION_H
#define WIMP_ACTION_H

#include "types.h"

bool get_action(char *name, action *action, char *args, void **data, char *response, int flag);
bool do_action(char *message, char *response);
void change_vt(void *data);  // this action is handled a little differently to the others
void actually_set_mark(const xkb_keysym_t sym);
void actually_go_to_mark(void *data);

// these actions are needed in other files
void pan_desk(void *data);
void toggle_fullscreen(void *data);

#endif
