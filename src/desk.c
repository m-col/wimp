#include "config.h"
#include "desk.h"
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
    struct desk *last = wl_container_of(wimp.desks.next, last, link);
    if (last == wimp.current_desk) {
	set_desk(wl_container_of(wimp.desks.next, last, link));
    }
    struct view *view, *tview;
    wl_list_for_each_safe(view, tview, &last->views, link) {
	view_to_desk(view, 0);
    };
    wl_list_init(&last->link);
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
    struct view *view;
    if (desk != wimp.current_desk) {
	wimp.can_steal_focus = false;
	wl_list_for_each(view, &wimp.current_desk->views, link) {
	    unmap_view(view);
	}
	wimp.current_desk = desk;
	wl_list_for_each(view, &wimp.current_desk->views, link) {
	    map_view(view);
	}
	wimp.can_steal_focus = true;
	if (!wl_list_empty(&desk->views)) {
	    view = wl_container_of(desk->views.next, view, link);
	    focus_view(view, view->surface->surface);
	}
    }
}


void view_to_desk(struct view *view, int index) {
    struct desk *desk;
    wimp.can_steal_focus = false;

    wl_list_for_each(desk, &wimp.desks, link) {
	if (desk->index == index) {
	    wl_list_remove(&view->link);
	    desk == wimp.current_desk ? map_view(view) : unmap_view(view);
	    wl_list_insert(&desk->views, &view->link);
	    break;
	}
    }

    wimp.can_steal_focus = true;
    if (!wl_list_empty(&wimp.current_desk->views)) {
	struct view *next_view = wl_container_of(wimp.current_desk->views.next, view, link);
	focus_view(next_view, next_view->surface->surface);
    }
}
