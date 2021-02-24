#ifndef WIMP_IPC_H
#define WIMP_IPC_H

#include "types.h"

void close_ipc(const char *display);
void set_up_defaults();
bool start_ipc(const char *display);

#endif
