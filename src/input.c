#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "input.h"
#include "types.h"


void on_cursor_motion(struct wl_listener *listener, void *data){
    return;
}


void on_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    return;
}


void on_cursor_button(struct wl_listener *listener, void *data) {
    return;
}

void on_cursor_axis(struct wl_listener *listener, void *data) {
    return;
}


void on_cursor_frame(struct wl_listener *listener, void *data) {
    return;
}


void on_new_input(struct wl_listener *listener, void *data) {
    return;
}


void on_request_cursor(struct wl_listener *listener, void *data) {
    return;
}


void on_request_set_selection(struct wl_listener *listener, void *data) {
    return;
}



void set_up_cursor(struct server *server) {
    server->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server->cursor, server->output_layout);
    server->cursor_manager = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(server->cursor_manager, 1);

    server->cursor_motion_listener.notify = on_cursor_motion;
    wl_signal_add(&server->cursor->events.motion, &server->cursor_motion_listener);

    server->cursor_motion_absolute_listener.notify = on_cursor_motion_absolute;
    wl_signal_add(&server->cursor->events.motion_absolute,
	&server->cursor_motion_absolute_listener);

    server->cursor_button_listener.notify = on_cursor_button;
    wl_signal_add(&server->cursor->events.button, &server->cursor_button_listener);

    server->cursor_axis_listener.notify = on_cursor_axis;
    wl_signal_add(&server->cursor->events.axis, &server->cursor_axis_listener);

    server->cursor_frame_listener.notify = on_cursor_frame;
    wl_signal_add(&server->cursor->events.frame, &server->cursor_frame_listener);
}


void set_up_keyboard(struct server *server) {
    wl_list_init(&server->keyboards);

    server->new_input_listener.notify = on_new_input;
    wl_signal_add(&server->backend->events.new_input, &server->new_input_listener);

    server->seat = wlr_seat_create(server->display, "seat0");
    server->request_cursor_listener.notify = on_request_cursor;
    wl_signal_add(&server->seat->events.request_set_cursor,
	    &server->request_cursor_listener);

    server->request_set_selection_listener.notify = on_request_set_selection;
    wl_signal_add(&server->seat->events.request_set_selection,
		    &server->request_set_selection_listener);
}
