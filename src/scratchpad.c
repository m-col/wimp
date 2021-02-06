#include "shell.h"
#include "scratchpad.h"
#include "types.h"


struct scratchpad *scratchpad_from_view(struct view *view) {
    struct scratchpad *scratchpad;

    wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	if (view == scratchpad->view) {
	    return scratchpad;
	}
    }

    return NULL;
}


void scratchpad_apply_geo(struct scratchpad *scratchpad) {
    struct wlr_output *output =
	wlr_output_layout_output_at(wimp.output_layout, wimp.cursor->x, wimp.cursor->y);
    struct wlr_box *ogeo = wlr_output_layout_get_box(wimp.output_layout, output);
    struct view *view = scratchpad->view;
    struct wlr_box *geo = &scratchpad->geo;

    if (geo->x > 0) {
	view->x = geo->x;
    } else {
	view->x = ogeo->x - (ogeo->width * geo->x / 100);
    }
    if (geo->y > 0) {
	view->y = geo->y;
    } else {
	view->y = ogeo->y - (ogeo->height * geo->y / 100);
    }

    struct wlr_box *sgeo = &view->surface->geometry;
    if (geo->width > 0) {
	sgeo->width = geo->width;
    } else {
	sgeo->width = - ogeo->width * geo->width / 100;
    }
    if (geo->height > 0) {
	sgeo->height = geo->height;
    } else {
	sgeo->height = - ogeo->height * geo->height / 100;
    }
    wlr_xdg_toplevel_set_size(view->surface, sgeo->width, sgeo->height);
}



bool catch_scratchpad(struct view *view) {
    pid_t pid;
    wl_client_get_credentials(view->surface->client->client, &pid, NULL, NULL);

    struct scratchpad *scratchpad;
    wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	if (scratchpad->pid == pid) {
	    scratchpad->view = view;
	    view->is_scratchpad = true;
	    map_view(view);
	    return true;
	}
    }
    return false;
}
