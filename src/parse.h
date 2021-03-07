#ifndef WIMP_PARSE_H
#define WIMP_PARSE_H

#include "types.h"

int _get(struct value_map *values, const int len, const char *name);
#define get(arr, name) _get(arr, sizeof(arr) / sizeof(arr[0]), name)

bool wlr_box_from_str(char* str, struct wlr_box *box);
bool dir_handler(void **data, char *args);
bool str_handler(void **data, char *args);
bool motion_handler(void **data, char *args);
bool scratchpad_handler(void **data, char *args);
bool box_handler(void **data, char *args);

#endif
