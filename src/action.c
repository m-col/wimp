#include <sys/vt.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_output_layout.h>

#include "action.h"
#include "config.h"
#include "cursor.h"
#include "desk.h"
#include "shell.h"
#include "types.h"


void shutdown(void *data) {
    wl_display_terminate(wimp.display);
}


void exec_command(void *data) {
    if (fork() == 0) {
	if (execl("/bin/sh", "/bin/sh", "-c", data, (void *)NULL) == -1) {
	    exit(EXIT_FAILURE);
	}
    } else {
	wlr_log(WLR_DEBUG, "Executing: %s", data);
    }
}


void change_vt(void *data) {
    unsigned vt = *(unsigned*)data;
    struct wlr_session *session = wlr_backend_get_session(wimp.backend);
    wlr_session_change_vt(session, vt);
}


void close_window(void *data) {
    struct wlr_surface *surface = wimp.seat->keyboard_state.focused_surface;
    if (surface) {
	struct wlr_xdg_surface *xdg_surface = surface->role_data;
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->toplevel) {
	    xdg_toplevel_send_close(xdg_surface->toplevel->resource);
	}
    }
}


void move_window(void *data) {
    struct view *view;
    if (wimp.grabbed_view) {
	view = wimp.grabbed_view;
    } else {
	double sx, sy;
	struct wlr_surface *surface;
	bool is_layer;
	view = under_pointer(&surface, &sx, &sy, &is_layer);
	if (!view || is_layer) {
	    return;
	}
    }
    struct motion motion = *(struct motion*)data;
    view->x += motion.dx;
    view->y += motion.dy;
}


void focus_in_direction(void *data) {
    if (wl_list_empty(&wimp.current_desk->views))
	return;

    struct view *current = wl_container_of(wimp.current_desk->views.next, current, link);
    enum direction dir = *(enum direction*)data;

    struct view *next = NULL;
    struct view *view;
    double vx, vy, dx, dy, vdist, dist;
    double x = current->x + current->surface->geometry.width / 2;
    double y = current->y + current->surface->geometry.height / 2;
    double c = y - x;  // y = x + c    / slope
    double n = y + x;  // y = n - x    \ slope
    bool above_c = dir & (DOWN | LEFT);
    bool above_n = dir & (DOWN | RIGHT);

    wl_list_for_each(view, &wimp.current_desk->views, link) {
	vx = view->x + view->surface->geometry.width / 2;
	vy = view->y + view->surface->geometry.height / 2;
	dx = x - vx;
	dy = y - vy;
	vdist = dx * dx + dy * dy;
	if (
	    (above_c ^ (vy - vx < c)) &&
	    (above_n ^ (vy + vx < n)) &&
	    (!next || vdist < dist) &&
	    view != current
	) {
	    next = view;
	    dist = vdist;
	}
    }

    if (next) {
	unfullscreen();
	pan_to_view(next);
	focus_view(next, next->surface->surface);
    }
}


void next_desk(void *data) {
    struct desk *desk;
    if (wimp.current_desk->index + 1 == wimp.desk_count) {
	desk = wl_container_of(wimp.desks.next, desk, link);
    } else {
	desk = wl_container_of(wimp.current_desk->link.next, desk, link);
    }
    set_desk(desk);
}


void prev_desk(void *data) {
    struct desk *desk;
    if (wimp.current_desk->index == 0) {
	desk = wl_container_of(wimp.desks.prev, desk, link);
    } else {
	desk = wl_container_of(wimp.current_desk->link.prev, desk, link);
    }
    set_desk(desk);
}


void pan_desk(void *data) {
    struct desk *desk = wimp.current_desk;
    struct motion motion = *(struct motion*)data;
    double dx = motion.dx / desk->zoom;
    double dy = motion.dy / desk->zoom;
    if (motion.is_percentage) {
	struct wlr_box *extents = wlr_output_layout_get_box(wimp.output_layout, NULL);
	dx = extents->width * (dx / 100);
	dy = extents->height * (dy / 100);
    }
    unfullscreen();
    struct view *view;
    wl_list_for_each(view, &desk->views, link) {
	view->x -= dx;
	view->y -= dy;
    }
    desk->panned_x -= dx;
    desk->panned_y -= dy;
}


void reset_zoom(void *data) {
    struct desk *desk = wimp.current_desk;
    double f = 1 / desk->zoom;
    if (f == 1) {
	return;
    }
    unfullscreen();
    struct wlr_box *extents = wlr_output_layout_get_box(wimp.output_layout, NULL);
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


void zoom(void *data) {
    /* Passed value (data) is the percentage step (+ve or -ve) */
    struct desk *desk = wimp.current_desk;
    double f = 1 + (*(double*)data / 100);
    if (
	(f > 1 && desk->zoom >= wimp.zoom_max) ||
	(f < 1 && desk->zoom <= wimp.zoom_min)
    ) {
	return;
    }
    unfullscreen();
    desk->zoom *= f;
    double fx = wimp.cursor->x * (f - 1) / desk->zoom;
    double fy = wimp.cursor->y * (f - 1) / desk->zoom;
    struct view *view;
    wl_list_for_each(view, &desk->views, link) {
	view->x -= fx;
	view->y -= fy;
    }
    desk->panned_x -= fx;
    desk->panned_y -= fy;
}


void zoom_scroll(void *data) {
    struct motion motion = *(struct motion*)data;
    double dz = - motion.dx - motion.dy;
    zoom(&dz);
}


static double zoom_pinch_initial;


void zoom_pinch(void *data) {
    double scale = *(double*)data;
    double dz = 100 * scale * zoom_pinch_initial / wimp.current_desk->zoom - 100;
    zoom(&dz);
}


void zoom_pinch_begin(void *data) {
    zoom_pinch_initial = wimp.current_desk->zoom;
}


void set_mark(void *data) {
    struct mark *mark = calloc(1, sizeof(struct mark));
    struct desk *desk = wimp.current_desk;
    mark->desk = desk;
    mark->x = desk->panned_x;
    mark->y = desk->panned_y;
    mark->zoom = desk->zoom;
    mark->key = 0;
    wimp.mark_waiting = true;
    wl_list_insert(&wimp.marks, &mark->link);
}


void actually_set_mark(const xkb_keysym_t sym) {
    struct mark *mark;
    struct mark *existing = NULL;

    if (sym == XKB_KEY_Escape) {
	mark = wl_container_of(wimp.marks.next, mark, link);
	wl_list_remove(&mark->link);
	free(mark);
	return;
    }

    wl_list_for_each(mark, &wimp.marks, link) {
	if (mark->key == sym) {
	    existing = mark;
	    break;
	}
    }
    if (existing) {
	wl_list_remove(&existing->link);
	free(existing);
    }

    mark = wl_container_of(wimp.marks.next, mark, link);
    mark->key = sym;
}


void go_to_mark(void *data) {
    wimp.mark_waiting = true;
}


void actually_go_to_mark(const xkb_keysym_t sym) {
    struct mark *mark;

    if (sym == XKB_KEY_Escape)
	return;

    wl_list_for_each(mark, &wimp.marks, link) {
	if (mark->key == sym) {
	    break;
	}
    }

    unfullscreen();
    struct motion motion = {
	.dx = mark->desk->panned_x - mark->x,
	.dy = mark->desk->panned_y - mark->y,
	.is_percentage = false,
    };
    mark->desk->zoom = mark->zoom;
    set_desk(mark->desk);
    pan_desk(&motion);

    struct view *view;
    struct wlr_box *extents = wlr_output_layout_get_box(wimp.output_layout, NULL);
    wl_list_for_each(view, &wimp.current_desk->views, link) {
	if (
	    view->x + view->surface->geometry.width < 0 || extents->width < view->x ||
	    view->y + view->surface->geometry.height < 0 || extents->height < view->y
	) {
	    continue;
	}
	focus_view(view, view->surface->surface);
	return;
    }
}


void toggle_fullscreen(void *data) {
    struct wlr_surface *surface = wimp.seat->keyboard_state.focused_surface;
    if (!surface || !wlr_surface_is_xdg_surface(surface)) {
	return;
    }
    struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
    struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
    fullscreen_xdg_surface(view, xdg_surface, NULL);
}


void halfimize(void *data) {
    if (wl_list_empty(&wimp.current_desk->views))
	return;

    struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
    enum direction dir = *(enum direction*)data;
    double vx = view->x + view->surface->geometry.width / 2;
    double vy = view->y + view->surface->geometry.height / 2;
    double x, y, width, height;
    wlr_output_layout_closest_point(wimp.output_layout, NULL, vx, vy, &x, &y);
    struct wlr_output *output = wlr_output_layout_output_at(wimp.output_layout, x, y);
    unfullscreen();

    switch (dir) {
	case RIGHT:
	    x = output->width / 2;
	    y = 0;
	    width = output->width / 2;
	    height = output->height;
	    break;
	case LEFT:
	    x = 0;
	    y = 0;
	    width = output->width / 2;
	    height = output->height;
	    break;
	case UP:
	    x = 0;
	    y = 0;
	    width = output->width;
	    height = output->height / 2;
	    break;
	case DOWN:
	    x = 0;
	    y = output->height / 2;
	    width = output->width;
	    height = output->height / 2;
	    break;
	case NONE:
	    return;
    }

    double zoom = wimp.current_desk->zoom;
    int border_width = wimp.current_desk->border_width;
    view->x = x / zoom + border_width;
    view->y = y / zoom + border_width;
    width -= border_width * 2;
    height -= border_width * 2;
    wlr_xdg_toplevel_set_size(view->surface, width / zoom, height / zoom);
    wlr_xdg_toplevel_set_tiled(view->surface, true);
}


void maximize(void *data) {
    if (wl_list_empty(&wimp.current_desk->views))
	return;

    struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
    double vx = view->x + view->surface->geometry.width / 2;
    double vy = view->y + view->surface->geometry.height / 2;
    double x, y;
    wlr_output_layout_closest_point(wimp.output_layout, NULL, vx, vy, &x, &y);
    struct wlr_output *output = wlr_output_layout_output_at(wimp.output_layout, x, y);
    unfullscreen();

    double zoom = wimp.current_desk->zoom;
    int border_width = wimp.current_desk->border_width;
    view->x = border_width;
    view->y = border_width;
    double width = (output->width - border_width * 2) / zoom;
    double height = (output->height - border_width * 2) / zoom;
    wlr_xdg_toplevel_set_size(view->surface, width, height);
    wlr_xdg_toplevel_set_tiled(view->surface, true);
}


void reload_config(void *data) {
    load_config();
}


void send_to_desk(void *data) {
    struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
    int index = *(double*)data - 1;
    unfullscreen();
    view_to_desk(view, index);
}
