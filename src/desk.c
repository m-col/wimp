#include "config.h"
#include "desk.h"
#include "shell.h"
#include "types.h"


static int num_desks = 0;


void add_desk(struct server *server) {
    struct desk *desk = calloc(1, sizeof(struct desk));
    wl_list_insert(server->desks.prev, &desk->link);
    desk->server = server;
    wl_list_init(&desk->views);
    assign_colour("#5D479D", desk->background);
    desk->wallpaper = NULL;
    desk->index = num_desks;
    desk->zoom = 1;
    num_desks++;
}


void remove_desk(struct desk *desk) {
    wl_list_remove(&desk->link);
    free(desk);
}


void set_desk(struct server *server, struct desk *desk) {
    struct view *view;
    wl_list_for_each(view, &server->current_desk->views, link) {
	unmap_view(view);
    }
    server->current_desk = desk;
    wl_list_for_each(view, &server->current_desk->views, link) {
	map_view(view);
    }
}


void next_desk(struct server *server, void *data) {
    struct desk *desk;
    if (server->current_desk->index + 1 == num_desks) {
	desk = wl_container_of(server->desks.next, desk, link);
    } else {
	desk = wl_container_of(server->current_desk->link.next, desk, link);
    }
    set_desk(server, desk);
}


void prev_desk(struct server *server, void *data) {
    struct desk *desk;
    if (server->current_desk->index == 0) {
	desk = wl_container_of(server->desks.prev, desk, link);
    } else {
	desk = wl_container_of(server->current_desk->link.prev, desk, link);
    }
    set_desk(server, desk);
}


void reset_pan(struct server *server, void *data) {
    struct desk *desk = server->current_desk;
    struct view *view;
    wl_list_for_each(view, &desk->views, link) {
	view->x -= desk->panned_x;
	view->y -= desk->panned_y;
    }
    desk->panned_x = desk->panned_y = 0;
    desk->zoom = 1;
}


void save_pan(struct server *server, void *data) {
    server->current_desk->panned_x = server->current_desk->panned_y = 0;
}


void zoom_desk(struct server *server, void *data) {
    /* dir > 0 ? zoom in : zoom out */
    struct desk *desk = server->current_desk;
    int dir = *(int*)data;
    double f = dir > 0 ? 1.015 : 1/1.015;
    if (
	(f > 1 && desk->zoom >= server->zoom_max) ||
	(f < 1 && desk->zoom <= server->zoom_min)
    ) {
	return;
    }
    desk->zoom *= f;
    double fx = server->cursor->x * (f - 1) / desk->zoom;
    double fy = server->cursor->y * (f - 1) / desk->zoom;
    struct view *view;
    wl_list_for_each(view, &desk->views, link) {
	view->x -= fx;
	view->y -= fy;
    }
    desk->panned_x -= fx;
    desk->panned_y -= fy;
}
