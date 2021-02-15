#ifndef WIMP_IPC_H
#define WIMP_IPC_H

#include "types.h"

void close_ipc(const char *display);
bool start_ipc(const char *display);

#endif
