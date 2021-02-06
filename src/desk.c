#include "config.h"
#include "desk.h"
#include "scratchpad.h"
#include "shell.h"
#include "types.h"


static void add_desk() {
    struct desk *desk = calloc(1, sizeof(struct desk));
    wl_list_insert(wimp.desks.prev, &desk->link);
    wl_list_init(&desk->views);
    assign_colour("#5D479D", desk->background);
    assign_colour("#3e3e73", desk->border_normal);
    assign_colour("#998dd1", desk->border_focus);
    desk->border_width = 4;
    desk->wallpaper = NULL;
    desk->index = wimp.desk_count;
    desk->zoom = 1;
    desk->fullscreened = NULL;
    wimp.desk_count++;

    if (!wimp.current_desk) {
	wimp.current_desk = wl_container_of(wimp.desks.next, wimp.current_desk, link);
    }
}


static void remove_desk() {
    struct desk *last = wl_container_of(wimp.desks.prev, last, link);
    if (last == wimp.current_desk) {
	set_desk(wl_container_of(wimp.desks.next, last, link));
    }
    struct view *view, *tview;
    wl_list_for_each_safe(view, tview, &last->views, link) {
	view_to_desk(view, 0);
    };
    wl_list_remove(&last->link);
    free(last->wallpaper);
    free(last);
    wimp.desk_count--;
}


void configure_desks(int wanted) {
    while (wanted > wimp.desk_count) {
	add_desk();
    }
    while (wanted < wimp.desk_count) {
	remove_desk();
    }
}


void set_desk(struct desk *desk) {
    if (desk == wimp.current_desk) {
	return;
    }

    wimp.current_desk = desk;

    // If a scratchpad is focussed, keep it focussed.
    struct scratchpad *scratchpad;
    wl_list_for_each(scratchpad, &wimp.scratchpads, link) {
	if (scratchpad->view && scratchpad->is_mapped) {
	    if (scratchpad->view->surface->surface == wimp.seat->keyboard_state.focused_surface) {
		return;
	    }
	}
    }

    // Otherwise focus the next view on the new desktop if there is one.
    if (wl_list_empty(&desk->views)) {
	focus_view(NULL, NULL);
    } else {
	struct view *view = wl_container_of(desk->views.next, view, link);
	focus_view(view, view->surface->surface);
    }
}


static struct desk *desk_from_index(int index) {
    struct desk *desk;
    wl_list_for_each(desk, &wimp.desks, link) {
	if (desk->index == index) {
	    return desk;
	}
    }
    return NULL;
}


void view_to_desk(struct view *view, int index) {
    struct desk *desk = desk_from_index(index);
    if (desk) {
	wl_list_remove(&view->link);
	wl_list_insert(&desk->views, &view->link);
	if (!wl_list_empty(&wimp.current_desk->views)) {
	    struct view *next_view = wl_container_of(wimp.current_desk->views.next, view, link);
	    focus_view(next_view, next_view->surface->surface);
	}
    }
}
