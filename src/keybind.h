#ifndef WIMP_KEYBIND_H
#define WIMP_KEYBIND_H

void free_binding(struct binding *kb);
void add_binding(char *message, char *response);
void set_mod(char *message, char *response);

#endif
