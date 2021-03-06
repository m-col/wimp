#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>

#include "action.h"
#include "output.h"
#include "scratchpad.h"
#include "types.h"


void view_apply_geometry(struct view *view, struct wlr_box *new) {
    if (new->width <= 0 || new->height <= 0) {
	return;
    }

    struct wlr_box old = {
	.x = view->x,
	.y = view->y,
	.width = view->surface->geometry.width,
	.height = view->surface->geometry.height,
    };

    view->x = new->x;
    view->y = new->y,
    wlr_xdg_toplevel_set_size(view->surface, new->width, new->height);
    damage_box(&old, true);
    damage_box(new, true);
}


void unmap_view(struct view *view) {
    wl_signal_emit(&view->surface->events.unmap, view);
}


void map_view(struct view *view) {
    wl_signal_emit(&view->surface->events.map, view);
}


void focus(void *data, struct wlr_surface *surface, bool is_layer) {
    /* view can be NULL. In that case focus is removed from any focussed surface. */
    /* surface can be NULL. In that case focus goes to view->surface->surface. */
    struct layer_view *lview;
    struct view *view;
    if (data) {
	if (is_layer) {
	    lview = (struct layer_view *)data;
	    if (!lview->surface->current.keyboard_interactive) {
		return;
	    }
	    if (!surface) {
		surface = lview->surface->surface;
	    }
	} else {
	    view = (struct view *)data;
	    if (!surface) {
		surface = view->surface->surface;
	    }
	}
    }
    struct wlr_surface *prev_surface = wimp.seat->keyboard_state.focused_surface;

    if (surface == prev_surface) {
	return;
    }
    wlr_seat_keyboard_notify_clear_focus(wimp.seat);

    if (prev_surface && wlr_surface_is_xdg_surface(prev_surface)) {
	struct wlr_xdg_surface *prev_xdg_surface = wlr_xdg_surface_from_wlr_surface(prev_surface);
	wlr_xdg_toplevel_set_activated(prev_xdg_surface, false);
	damage_by_view(prev_xdg_surface->data, true);
    }

    if (!data) {
	wimp.focussed_layer_view = NULL;
	return;
    }

    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(wimp.seat);

    if (is_layer) {
	if (!lview->surface->mapped || !lview->surface->current.keyboard_interactive) {
	    return;
	}
	if (!surface) {
	    surface = lview->surface->surface;
	}
	wlr_seat_keyboard_notify_enter(
	    wimp.seat, surface, keyboard->keycodes,
	    keyboard->num_keycodes, &keyboard->modifiers
	);
	wlr_surface_send_enter(surface, lview->surface->output);
	wimp.focussed_layer_view = lview;
	damage_by_lview(lview);

    } else {
	if (view->is_scratchpad) {
	    struct scratchpad *scratchpad = scratchpad_from_view(view);
	    scratchpad->is_mapped = true;
	} else {
	    wl_list_remove(&view->link);
	    wl_list_insert(&wimp.current_desk->views, &view->link);
	}
	if (!surface) {
	    surface = view->surface->surface;
	}
	if (wlr_surface_is_xdg_surface(surface)) {
	    wlr_xdg_toplevel_set_activated(view->surface, true);
	}
	wlr_seat_keyboard_notify_enter(
	    wimp.seat, surface, keyboard->keycodes,
	    keyboard->num_keycodes, &keyboard->modifiers
	);
	damage_by_view(view, true);
    }
}


void focus_view(struct view *view, struct wlr_surface *surface) {
    focus(view, surface, false);
}


void find_focus() {
    struct scratchpad *scratchpad;
    wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	if (scratchpad->is_mapped) {
	    focus_view(scratchpad->view, NULL);
	    return;
	}
    }

    if (!wl_list_empty(&wimp.current_desk->views)) {
	struct view *view = wl_container_of(wimp.current_desk->views.next, view, link);
	focus_view(view, NULL);
	return;
    };

    focus_view(NULL, NULL);
}


void pan_to_view(struct view *view) {
    int border_width = wimp.current_desk->border_width;
    double x = view->x - border_width;
    double y = view->y - border_width;
    double width = view->surface->geometry.width + border_width * 2;
    double height = view->surface->geometry.height + border_width * 2;
    struct wlr_box *extents = wlr_output_layout_get_box(wimp.output_layout, NULL);
    double zoom = wimp.current_desk->zoom;
    struct motion motion = {
	.dx = 0,
	.dy = 0,
	.is_percentage = false,
    };

    int panning = 2;
    if (x < 0) {
	motion.dx = x * zoom;
    } else if (x + width > extents->width / zoom) {
	motion.dx = (x + width) * zoom - extents->width;
    } else {
	panning -= 1;
    }
    if (y < 0) {
	motion.dy = y * zoom;
    } else if (y + height > extents->height / zoom) {
	motion.dy = (y + height) * zoom - extents->height;
    } else {
	panning -= 1;
    }
    if (panning) {
	pan_desk(&motion);
	damage_all_outputs();
    }
}


void fullscreen_xdg_surface(
    struct view *view, struct wlr_xdg_surface *xdg_surface, struct wlr_output *wlr_output
) {
    /* wlr_output can be NULL, in which case it is calculated using the surface's position */
    struct wlr_box *saved_geo = &wimp.current_desk->fullscreened_saved_geo;

    struct wlr_xdg_surface *prev_surface = wimp.current_desk->fullscreened;
    if (prev_surface) {
	wlr_xdg_toplevel_set_fullscreen(prev_surface, false);
	wlr_xdg_toplevel_set_tiled(view->surface, true);
	wlr_xdg_toplevel_set_size(prev_surface, saved_geo->width, saved_geo->height);
	view->x = saved_geo->x;
	view->y = saved_geo->y;
    }

    if (prev_surface == xdg_surface) {
	wimp.current_desk->fullscreened = NULL;
	return;
    }

    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
	return;

    if (!wlr_output) {
	double x = view->x + xdg_surface->geometry.width / 2;
	double y = view->y + xdg_surface->geometry.height / 2;
	double lx, ly;
	wlr_output_layout_closest_point(wimp.output_layout, NULL, x, y, &lx, &ly);
	wlr_output = wlr_output_layout_output_at(wimp.output_layout, lx, ly);
    }

    wimp.current_desk->fullscreened = xdg_surface;
    saved_geo->x = view->x;
    saved_geo->y = view->y;
    view->x = view->y = 0;
    saved_geo->width = xdg_surface->geometry.width;
    saved_geo->height = xdg_surface->geometry.height;
    wlr_xdg_toplevel_set_fullscreen(xdg_surface, true);
    wlr_xdg_toplevel_set_tiled(view->surface, false);
    double zoom = wimp.current_desk->zoom;
    wlr_xdg_toplevel_set_size(xdg_surface, wlr_output->width / zoom, wlr_output->height / zoom);
    struct output *output = wlr_output->data;
    wlr_output_damage_add_whole(output->wlr_output_damage);
}


void unfullscreen() {
    if (wimp.current_desk->fullscreened != NULL) {
	toggle_fullscreen(NULL);
    }
}


static void on_commit(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, commit_listener);
    damage_by_view(view, false);
}


static void on_map(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, map_listener);

    view->commit_listener.notify = on_commit;
    wl_signal_add(&view->surface->surface->events.commit, &view->commit_listener);

    if (view->is_scratchpad) {
	struct scratchpad *scratchpad = scratchpad_from_view(view);
	scratchpad_apply_geo(scratchpad);
	scratchpad->is_mapped = true;
    }

    wlr_xdg_toplevel_set_tiled(view->surface, true);
    focus_view(view, NULL);
}


static void on_unmap(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, unmap_listener);

    if (view->is_scratchpad) {
	struct scratchpad *scratchpad = scratchpad_from_view(view);
	scratchpad->is_mapped = false;
    } else {
	wl_list_remove(&view->link);
	wl_list_insert(wimp.current_desk->views.prev, &view->link);
    }

    wl_list_remove(&view->commit_listener.link);
    damage_by_view(view, true);

    if (view->surface->surface == wimp.seat->keyboard_state.focused_surface) {
	find_focus();
    }
}


static void on_surface_destroy(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, destroy_listener);
    if (wimp.current_desk->fullscreened == view->surface) {
	wimp.current_desk->fullscreened = NULL;
    }

    if (view->is_scratchpad) {
	struct scratchpad *scratchpad = scratchpad_from_view(view);
	scratchpad->is_mapped = false;
	scratchpad->view = NULL;
    } else {
	wl_list_remove(&view->link);
    }

    free(view);
}


static void process_move_resize(struct view *view, enum cursor_mode mode, uint32_t edges) {
    struct wlr_surface *focused_surface = wimp.seat->pointer_state.focused_surface;

    if (view->surface->surface != focused_surface) {
	    return;
    }
    wimp.grabbed_view = view;
    wimp.cursor_mode = mode;

    double zoom = wimp.current_desk->zoom;
    wimp.grab_x = wimp.cursor->x - view->x * zoom;
    wimp.grab_y = wimp.cursor->y - view->y * zoom;

    if (mode == CURSOR_RESIZE) {
	wlr_xdg_surface_get_geometry(view->surface, &wimp.grab_geobox);
	wimp.grab_geobox.x = view->x;
	wimp.grab_geobox.y = view->y;
	wimp.resize_edges = edges;
    }
}


static void on_request_move(struct wl_listener *listener, void *data) {
    struct view *view = wl_container_of(listener, view, request_move_listener);
    wlr_xdg_toplevel_set_tiled(view->surface, false);
    process_move_resize(view, CURSOR_MOVE, 0);
}


static void on_request_resize(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_resize_event *event = data;
    struct view *view = wl_container_of(listener, view, request_resize_listener);
    wlr_xdg_toplevel_set_tiled(view->surface, false);
    process_move_resize(view, CURSOR_RESIZE, event->edges);
}


static void on_request_fullscreen(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_set_fullscreen_event *event = data;
    struct view *view = wl_container_of(listener, view, request_fullscreen_listener);
    fullscreen_xdg_surface(view, event->surface, event->output);
}


static void on_new_xdg_surface(struct wl_listener *listener, void *data) {
    struct wlr_xdg_surface *surface = data;
    if (surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
	return;
    }

    struct view *view = calloc(1, sizeof(struct view));
    view->surface = surface;
    surface->data = view;

    view->map_listener.notify = on_map;
    wl_signal_add(&surface->events.map, &view->map_listener);
    view->unmap_listener.notify = on_unmap;
    wl_signal_add(&surface->events.unmap, &view->unmap_listener);
    view->destroy_listener.notify = on_surface_destroy;
    wl_signal_add(&surface->events.destroy, &view->destroy_listener);

    struct wlr_xdg_toplevel *toplevel = surface->toplevel;
    view->request_move_listener.notify = on_request_move;
    wl_signal_add(&toplevel->events.request_move, &view->request_move_listener);
    view->request_resize_listener.notify = on_request_resize;
    wl_signal_add(&toplevel->events.request_resize, &view->request_resize_listener);
    view->request_fullscreen_listener.notify = on_request_fullscreen;
    wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen_listener);

    if (wimp.scratchpad_waiting) {
	if (catch_scratchpad(view)) {
	    return;
	}
    }

    view->is_scratchpad = false;
    view->x = wimp.current_desk->border_width;
    view->y = wimp.current_desk->border_width;
    wl_list_insert(&wimp.current_desk->views, &view->link);
}


void set_up_shell() {
    wimp.shell = wlr_xdg_shell_create(wimp.display);
    wimp.new_xdg_surface_listener.notify = on_new_xdg_surface;
    wl_signal_add(&wimp.shell->events.new_surface, &wimp.new_xdg_surface_listener);
}
