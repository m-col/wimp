#include "config.h"
#include "desk.h"
#include "types.h"

static int num_desks = 0;


void add_desk(struct server *server) {
    struct desk *desk = calloc(1, sizeof(struct desk));
    wl_list_insert(server->desks.prev, &desk->link);
    desk->server = server;
    wl_list_init(&desk->views);
    assign_colour("#5D479D", desk->background);
    desk->index = num_desks;
    num_desks++;
}


void remove_desk(struct desk *desk) {
    wl_list_remove(&desk->link);
    free(desk);
}


void next_desk(struct server *server) {
    struct desk *desk;
    if (server->current_desk->index + 1 == num_desks) {
	desk = wl_container_of(server->desks.next, desk, link);
    } else {
	desk = wl_container_of(server->current_desk->link.next, desk, link);
    }
    server->current_desk = desk;
}


void prev_desk(struct server *server) {
    struct desk *desk;
    if (server->current_desk->index == 0) {
	desk = wl_container_of(server->desks.prev, desk, link);
    } else {
	desk = wl_container_of(server->current_desk->link.prev, desk, link);
    }
    server->current_desk = desk;
}
