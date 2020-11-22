#include <sys/vt.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>

#include "desk.h"
#include "types.h"


void shutdown(struct server *server, void *data) {
    wl_display_terminate(server->display);
}


void exec_command(struct server *server, void *data) {
    if (fork() == 0) {
	execl("/bin/sh", "/bin/sh", "-c", data, (void *)NULL);
    }
}


void change_vt(struct server *server, void *data) {
    unsigned vt = *(unsigned*)data;
    struct wlr_session *session = wlr_backend_get_session(server->backend);
    wlr_session_change_vt(session, vt);
}


void close_current_window(struct server *server, void *data) {
    struct wlr_surface *surface = server->seat->keyboard_state.focused_surface;
    if (surface) {
	struct wlr_xdg_surface *xdg_surface = surface->role_data;
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->toplevel) {
	    xdg_toplevel_send_close(xdg_surface->toplevel->resource);
	}
    }
}


void next_desk(struct server *server, void *data) {
    struct desk *desk;
    if (server->current_desk->index + 1 == server->desk_count) {
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


void pan_desk(struct server *server, void *data) {
    struct desk *desk = server->current_desk;
    struct motion motion = *(struct motion*)data;
    double dx = motion.dx;
    double dy = motion.dy;
    struct view *view;
    wl_list_for_each(view, &desk->views, link) {
	view->x += dx;
	view->y += dy;
    }
    desk->panned_x += dx;
    desk->panned_y += dy;
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