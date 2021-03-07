#include <sys/vt.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_output_layout.h>

#include "action.h"
#include "config.h"
#include "cursor.h"
#include "desk.h"
#include "output.h"
#include "parse.h"
#include "scratchpad.h"
#include "shell.h"
#include "types.h"


static void terminate(void *data);
static void exec_command(void *data);
static void close_window(void *data);
static void move_window(void *data);
static void focus_in_direction(void *data);
static void next_desk(void *data);
static void prev_desk(void *data);
static void reset_zoom(void *data);
static void zoom(void *data);
static void set_mark(void *data);
static void go_to_mark(void *data);
static void halfimize(void *data);
static void maximize(void *data);
static void send_to_desk(void *data);
static void toggle_scratchpad(void *data);
static void to_region(void *data);
static void zoom_pinch(void *data);
static void zoom_pinch_begin(void *data);
static void zoom_scroll(void *data);


static struct {
    const char *name;
    const action action;
    const action begin;
} pinch_map[] = {
    { "zoom", &zoom_pinch, &zoom_pinch_begin },
};


static struct {
    const char *name;
    const action action;
} scroll_map[] = {
    { "zoom", &zoom_scroll },
    { "pan_desk", &pan_desk },
};


static struct {
    const char *name;
    const action action;
    bool (*data_handler)(void **data, char *args);
} action_map[] = {
    { "terminate", &terminate, NULL },
    { "exec", &exec_command, &str_handler },
    { "close_window", &close_window, NULL },
    { "move_window", &move_window, NULL },
    { "focus", &focus_in_direction, &dir_handler },
    { "next_desk", &next_desk, NULL },
    { "prev_desk", &prev_desk, NULL },
    { "pan_desk", &pan_desk, &motion_handler },
    { "reset_zoom", &reset_zoom, NULL },
    { "zoom", &zoom, &str_handler },
    { "set_mark", &set_mark, NULL },
    { "go_to_mark", &go_to_mark, NULL },
    { "toggle_fullscreen", &toggle_fullscreen, NULL },
    { "halfimize", &halfimize, &dir_handler },
    { "maximize", &maximize, NULL },
    { "send_to_desk", &send_to_desk, &str_handler },
    { "scratchpad", &toggle_scratchpad, &scratchpad_handler },
    { "to_region", &to_region, &box_handler },
};


bool get_action(
    char *name, action *found, char *args, void **data, char *response, int flag
) {
    // flag can be PINCH or SCROLL to access their actions, otherwise neither
    *data = NULL;
    size_t i;

    switch (flag) {
	case PINCH:
	    for (i = 0; i < sizeof(pinch_map) / sizeof(pinch_map[0]); i++) {
		if (strcmp(pinch_map[i].name, name) == 0) {
		    *found = pinch_map[i].action;
		    action begin = pinch_map[i].begin;
		    *data = calloc(1, sizeof(action));
		    *(action *)(*data) = begin;
		    return true;
		}
	    }
	    break;

	case SCROLL:
	    for (i = 0; i < sizeof(scroll_map) / sizeof(scroll_map[0]); i++) {
		if (strcmp(scroll_map[i].name, name) == 0) {
		    *found = scroll_map[i].action;
		    return true;
		}
	    }
	    break;

	default:
	    for (i = 0; i < sizeof(action_map) / sizeof(action_map[0]); i++) {
		if (strcmp(action_map[i].name, name) == 0) {
		    *found = action_map[i].action;

		    if (action_map[i].data_handler != NULL) {
			if (!action_map[i].data_handler(data, args)) {
			    sprintf(response, "Command malformed/incomplete.");
			    return false;
			}
		    }

		    return true;
		}
	    }
    }

    return false;
}


bool do_action(char *message, char *response) {
    void *data;
    action action;
    if (get_action(message, &action, strtok(NULL, "\n\r"), &data, response, 0)) {
	(*action)(data);
	return true;
    } else {
	return false;
    }
}


static void terminate(void *data) {
    wl_display_terminate(wimp.display);
}


static void exec_command(void *data) {
    pid_t pid = fork();
    if (pid == 0) {
	if (execl("/bin/sh", "/bin/sh", "-c", data, (void *)NULL) == -1) {
	    exit(EXIT_FAILURE);
	}
    } else if (pid < 0) {
	wlr_log(WLR_ERROR, "Failed to fork for exec command.");
    } else {
	wlr_log(WLR_DEBUG, "Executing: %s", (char *)data);
    }
}


void change_vt(void *data) {
    unsigned vt = *(unsigned *)data;
    struct wlr_session *session = wlr_backend_get_session(wimp.backend);
    wlr_session_change_vt(session, vt);
}


static void close_window(void *data) {
    struct wlr_surface *surface = wimp.seat->keyboard_state.focused_surface;
    if (surface) {
	struct wlr_xdg_surface *xdg_surface = surface->role_data;
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->toplevel) {
	    xdg_toplevel_send_close(xdg_surface->toplevel->resource);
	}
    }
}


static void move_window(void *data) {
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
    double zoom = wimp.current_desk->zoom;
    struct wlr_box damage = {
	.x = view->x * zoom,
	.y = view->y * zoom,
	.width = view->surface->geometry.width * zoom,
	.height = view->surface->geometry.height * zoom,
    };
    struct motion motion = *(struct motion*)data;
    if (motion.dx > 0) {
	damage.width += motion.dx;
    } else {
	damage.x -= motion.dx;
	damage.width -= motion.dx;
    }
    if (motion.dy > 0) {
	damage.height += motion.dy;
    } else {
	damage.y -= motion.dy;
	damage.height -= motion.dy;
    }
    view->x += motion.dx;
    view->y += motion.dy;
    damage_box(&damage, true);
}


static void focus_in_direction(void *data) {
    if (wl_list_empty(&wimp.current_desk->views)) {
	return;
    }

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
	focus_view(next, NULL);
    } else {
	pan_to_view(current);
	focus_view(current, NULL);
    }
}


static void next_desk(void *data) {
    struct desk *desk;
    if (wimp.current_desk->index + 1 == wimp.desk_count) {
	desk = wl_container_of(wimp.desks.next, desk, link);
    } else {
	desk = wl_container_of(wimp.current_desk->link.next, desk, link);
    }
    set_desk(desk);
}


static void prev_desk(void *data) {
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
    damage_all_outputs();
}


static void zoom(void *data) {
    /* Passed value (data) is the percentage step (+ve or -ve) */
    struct desk *desk = wimp.current_desk;
    double f = 1 + (*(double*)data / 100);

    double next_zoom = desk->zoom * f;
    if (next_zoom < wimp.zoom_min) {
	f = wimp.zoom_min / desk->zoom;
    } else if (next_zoom > wimp.zoom_max) {
	f = wimp.zoom_max / desk->zoom;
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
    damage_all_outputs();
}


static void reset_zoom(void *data) {
    double dz = 100 * 1 / wimp.current_desk->zoom - 100;
    zoom(&dz);
}


static void zoom_scroll(void *data) {
    struct motion motion = *(struct motion*)data;
    double dz = - motion.dx - motion.dy;
    zoom(&dz);
}


static double zoom_pinch_initial;


static void zoom_pinch(void *data) {
    double scale = *(double*)data;
    double dz = 100 * scale * zoom_pinch_initial / wimp.current_desk->zoom - 100;
    zoom(&dz);
}


static void zoom_pinch_begin(void *data) {
    zoom_pinch_initial = wimp.current_desk->zoom;
}


static void set_mark(void *data) {
    struct mark *mark = calloc(1, sizeof(struct mark));
    struct desk *desk = wimp.current_desk;
    mark->desk = desk;
    mark->x = desk->panned_x;
    mark->y = desk->panned_y;
    mark->zoom = desk->zoom;
    mark->key = 0;
    wl_list_insert(&wimp.marks, &mark->link);
    wimp.mark_waiting = true;
    damage_mark_indicator();
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

    if (wimp.bind_marks) {
	struct binding *kb, *tmp;
	wl_list_for_each_safe(kb, tmp, &wimp.key_bindings, link) {
	    if (kb->key == sym && kb->mods == 0) {
		return;
	    }
	}
	kb = calloc(1, sizeof(struct binding));
	kb->mods = 0;
	kb->key = sym;
	kb->data = calloc(1, sizeof(xkb_keysym_t));
	kb->action = &actually_go_to_mark;
	*(xkb_keysym_t *)(kb->data) = sym;
	wl_list_insert(&wimp.key_bindings, &kb->link);
    }
}


static void go_to_mark(void *data) {
    wimp.mark_waiting = true;
    damage_mark_indicator();
}


void actually_go_to_mark(void *data) {
    const xkb_keysym_t sym = *(xkb_keysym_t *)data;
    struct mark *mark;

    if (sym == XKB_KEY_Escape) {
	return;
    }

    bool found = false;
    wl_list_for_each(mark, &wimp.marks, link) {
	if (mark->key == sym) {
	    found = true;
	    break;
	}
    }
    if (!found) {
	return;
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
	focus_view(view, NULL);
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


static void halfimize(void *data) {
    struct wlr_surface *surface = wimp.seat->keyboard_state.focused_surface;
    if (!surface || !wlr_surface_is_xdg_surface(surface) || wl_list_empty(&wimp.current_desk->views)) {
	return;
    }
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
	case NO_DIRECTION:
	    return;
    }

    double zoom = wimp.current_desk->zoom;
    int border_width = wimp.current_desk->border_width;
    struct wlr_box new = {
	.x = x / zoom + border_width,
	.y = y / zoom + border_width,
	.width = (width - border_width * 2) / zoom,
	.height = (height - border_width * 2) / zoom,
    };
    view_apply_geometry(view, &new);
}


static void maximize(void *data) {
    struct wlr_surface *surface = wimp.seat->keyboard_state.focused_surface;
    if (!surface || !wlr_surface_is_xdg_surface(surface) || wl_list_empty(&wimp.current_desk->views)) {
	return;
    }

    struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
    double vx = view->x + view->surface->geometry.width / 2;
    double vy = view->y + view->surface->geometry.height / 2;
    double x, y;
    wlr_output_layout_closest_point(wimp.output_layout, NULL, vx, vy, &x, &y);
    struct wlr_output *output = wlr_output_layout_output_at(wimp.output_layout, x, y);
    unfullscreen();

    double zoom = wimp.current_desk->zoom;
    int border_width = wimp.current_desk->border_width;
    struct wlr_box new = {
	.x = border_width,
	.y = border_width,
	.width = (output->width - border_width * 2) / zoom,
	.height = (output->height - border_width * 2) / zoom,
    };
    view_apply_geometry(view, &new);
}


static void send_to_desk(void *data) {
    struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
    int index = *(double*)data - 1;
    unfullscreen();
    view_to_desk(view, index);
}


static void toggle_scratchpad(void *data) {
    struct scratchpad *scratchpad = scratchpad_from_id(*(int *)data);

    if (scratchpad->view) {
	if (scratchpad->is_mapped) {
	    unmap_view(scratchpad->view);
	} else {
	    map_view(scratchpad->view);
	}
	return;
    }

    wimp.scratchpad_waiting = true;

    scratchpad->pid = fork();
    if (scratchpad->pid == 0) {
	execl("/bin/sh", "/bin/sh", "-c", scratchpad->command, (void *)NULL);
	exit(EXIT_FAILURE);
    } else if (scratchpad->pid < 0) {
	wlr_log(WLR_ERROR, "Failed to fork for scratchpad.");
	wimp.scratchpad_waiting = false;
    } else {
	wlr_log(WLR_DEBUG, "Launching scratchpad: %s", scratchpad->command);
    }
}


static void to_region(void *data) {
    struct wlr_box *box = (struct wlr_box *)data;
    struct wlr_box *extents = wlr_output_layout_get_box(wimp.output_layout, NULL);

    struct motion motion = {
	.dx = - (extents->width / 2) + (box->width / 2) + box->x,
	.dy = - (extents->height / 2) + (box->height / 2) + box->y,
	.is_percentage = false,
    };
    pan_desk(&motion);

    double f;
    if (extents->width / extents->height > box->width / box->height) {
	f = wimp.current_desk->zoom / ((double)box->width / (double)extents->width);
    } else {
	f = wimp.current_desk->zoom / ((double)box->height / (double)extents->height);
    }
    double dz = 100 * f / wimp.current_desk->zoom - 100;
    double cx = wimp.cursor->x;
    double cy = wimp.cursor->y;
    wimp.cursor->x = extents->width / 2;
    wimp.cursor->y = extents->height / 2;
    zoom(&dz);
    wimp.cursor->x = cx;
    wimp.cursor->y = cy;
}
