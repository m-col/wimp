#include <sys/vt.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_output_layout.h>

#include "action.h"
#include "desk.h"
#include "shell.h"
#include "types.h"

#define DIST(x, y) ((x) * (x) + (y) * (y))


void shutdown(struct server *server, void *data) {
    wl_display_terminate(server->display);
}


void exec_command(struct server *server, void *data) {
    if (fork() == 0) {
	execl("/bin/sh", "/bin/sh", "-c", data, (void *)NULL);
    } else {
	wlr_log(WLR_DEBUG, "Executing: %s", data);
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


void focus_in_direction(struct server *server, void *data) {
    struct view *current = wl_container_of(server->current_desk->views.next, current, link);
    enum direction dir = *(enum direction*)data;

    double x = current->x + current->surface->geometry.width / 2;
    double y = current->y + current->surface->geometry.height / 2;
    double c = y - x;  // y = x + c    / slope
    double n = y + x;  // y = n - x    \ slope

    struct view *next = NULL;
    struct view *view;
    double vx, vy, vdist, dist;

    switch (dir) {
	case RIGHT:
	    wl_list_for_each(view, &server->current_desk->views, link) {
		vx = view->x + view->surface->geometry.width / 2;
		vy = view->y + view->surface->geometry.height / 2;
		vdist = DIST(x - vx, y - vy);
		if (vy - vx < c && vy + vx > n && (!next || vdist < dist)) {
		    next = view;
		    dist = vdist;
		}
	    }
	    break;
	case LEFT:
	    wl_list_for_each(view, &server->current_desk->views, link) {
		vx = view->x + view->surface->geometry.width / 2;
		vy = view->y + view->surface->geometry.height / 2;
		vdist = DIST(x - vx, y - vy);
		if (vy - vx > c && vy + vx < n && (!next || vdist < dist)) {
		    next = view;
		    dist = vdist;
		}
	    }
	    break;
	case DOWN:
	    wl_list_for_each(view, &server->current_desk->views, link) {
		vx = view->x + view->surface->geometry.width / 2;
		vy = view->y + view->surface->geometry.height / 2;
		vdist = DIST(x - vx, y - vy);
		if (vy - vx > c && vy + vx > n && (!next || vdist < dist)) {
		    next = view;
		    dist = vdist;
		}
	    }
	    break;
	case UP:
	    wl_list_for_each(view, &server->current_desk->views, link) {
		vx = view->x + view->surface->geometry.width / 2;
		vy = view->y + view->surface->geometry.height / 2;
		vdist = DIST(x - vx, y - vy);
		if (vy - vx < c && vy + vx < n && (!next || vdist < dist)) {
		    next = view;
		    dist = vdist;
		}
	    }
	    break;
	case NONE:
	    return;
    }
    if (next)
	focus_view(next, next->surface->surface);
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


void reset_zoom(struct server *server, void *data) {
    struct desk *desk = server->current_desk;
    double f = 1 / desk->zoom;
    struct wlr_box *extents = wlr_output_layout_get_box(server->output_layout, NULL);
    double fx = extents->width * (f - 1) / 2;
    double fy = extents->height * (f - 1) / 2;
    struct view *view;
    wl_list_for_each(view, &desk->views, link) {
	view->x -= fx;
	view->y -= fy;
    }
    desk->panned_x -= fx;
    desk->panned_y -= fy;
    desk->zoom = 1;
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


void zoom_desk_mouse(struct server *server, void *data) {
    struct motion motion = *(struct motion*)data;
    int dir = motion.dx + motion.dy;
    zoom_desk(server, &dir);
}


void set_mark(struct server *server, void *data) {
    struct mark *mark = calloc(1, sizeof(struct mark));
    struct desk *desk = server->current_desk;
    mark->desk = desk;
    mark->x = desk->panned_x;
    mark->y = desk->panned_y;
    mark->zoom = desk->zoom;
    mark->key = 0;
    server->mark_waiting = true;
    wl_list_insert(&server->marks, &mark->link);
}


void actually_set_mark(struct server *server, const xkb_keysym_t sym) {
    struct mark *mark;
    struct mark *existing = NULL;

    if (sym == XKB_KEY_Escape) {
	mark = wl_container_of(server->marks.next, mark, link);
	wl_list_remove(&mark->link);
	free(mark);
	return;
    }

    wl_list_for_each(mark, &server->marks, link) {
	if (mark->key == sym) {
	    existing = mark;
	    break;
	}
    }
    if (existing) {
	wl_list_remove(&existing->link);
	free(existing);
    }

    mark = wl_container_of(server->marks.next, mark, link);
    mark->key = sym;
}


void go_to_mark(struct server *server, void *data) {
    server->mark_waiting = true;
}


void actually_go_to_mark(struct server *server, const xkb_keysym_t sym) {
    struct mark *mark;

    if (sym == XKB_KEY_Escape)
	return;

    wl_list_for_each(mark, &server->marks, link) {
	if (mark->key == sym) {
	    struct motion motion = {
		.dx = mark->x - mark->desk->panned_x,
		.dy = mark->y - mark->desk->panned_y,
	    };
	    mark->desk->zoom = mark->zoom;
	    set_desk(server, mark->desk);
	    pan_desk(server, &motion);
	    return;
	}
    }
}


void toggle_fullscreen(struct server *server, void *data) {
    struct wlr_surface *surface = server->seat->keyboard_state.focused_surface;
    if (!surface)
	return;
    struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
    struct view *view = wl_container_of(server->current_desk->views.next, view, link);
    fullscreen_xdg_surface(view, xdg_surface, NULL);
}
