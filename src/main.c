#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "cursor.h"
#include "decorations.h"
#include "desk.h"
#include "main.h"
#include "input.h"
#include "layer_shell.h"
#include "log.h"
#include "output.h"
#include "scratchpad.h"
#include "shell.h"
#include "types.h"


struct wimp wimp = {
    .on_mouse_motion = NULL,
    .on_mouse_scroll = NULL,
    .on_drag1 = NULL,
    .on_drag2 = NULL,
    .on_drag3 = NULL,
    .on_pinch = NULL,
    .desk_count = 0,
    .mark_waiting = false,
    .mark_indicator.box.width = 25,
    .mark_indicator.box.height = 25,
    .mark_indicator.box.x = 0,
    .mark_indicator.box.y = 0,
    .scratchpad_waiting = false,
    .auto_focus = false,
};


static const char usage[] =
    "usage: wimp [options...]\n"
    "    -h         show this help message\n"
    "    -c <file>  specify config file\n"
    "    -d         set logging to debug mode\n"
    "    -i         set logging to info mode\n"
;


static void shutdown() {
    free(wimp.config_directory);
    free(wimp.config_file);
    wl_list_remove(&wimp.new_output_listener.link);
    wl_list_remove(&wimp.new_xdg_surface_listener.link);
    wl_list_remove(&wimp.decoration_listener.link);
    wl_list_remove(&wimp.layer_shell_surface_listener.link);
    wl_list_remove(&wimp.cursor_motion_listener.link);
    wl_list_remove(&wimp.cursor_motion_absolute_listener.link);
    wl_list_remove(&wimp.cursor_button_listener.link);
    wl_list_remove(&wimp.cursor_axis_listener.link);
    wl_list_remove(&wimp.cursor_frame_listener.link);
    wl_list_remove(&wimp.new_input_listener.link);
    wl_list_remove(&wimp.request_cursor_listener.link);
    wl_list_remove(&wimp.request_set_selection_listener.link);

    drop_scratchpads();

    struct binding *kb, *tkb;
    wl_list_for_each_safe(kb, tkb, &wimp.mouse_bindings, link) {
	wl_list_remove(&kb->link);
	if (kb->data) {
	    free(kb->data);
	}
	free(kb);
    };
    wl_list_for_each_safe(kb, tkb, &wimp.key_bindings, link) {
	wl_list_remove(&kb->link);
	if (kb->data) {
	    free(kb->data);
	}
	free(kb);
    };

    struct desk *desk, *tdesk;
    struct view *view, *tview;
    wl_list_for_each_safe(desk, tdesk, &wimp.desks, link) {
	wl_list_for_each_safe(view, tview, &desk->views, link) {
	    wl_list_remove(&view->link);
	    wl_list_remove(&view->map_listener.link);
	    wl_list_remove(&view->unmap_listener.link);
	    wl_list_remove(&view->destroy_listener.link);
	    wl_list_remove(&view->request_move_listener.link);
	    wl_list_remove(&view->request_resize_listener.link);
	    wl_list_remove(&view->request_fullscreen_listener.link);
	    free(view);
	};
	wl_list_remove(&desk->link);
	free(desk->wallpaper);
	free(desk);
    };

    struct mark *mark, *tmark;
    wl_list_for_each_safe(mark, tmark, &wimp.marks, link) {
	wl_list_remove(&mark->link);
	free(mark);
    };

    wlr_backend_destroy(wimp.backend);
    wlr_output_layout_destroy(wimp.output_layout);
    wlr_seat_destroy(wimp.seat);
    wlr_xcursor_manager_destroy(wimp.cursor_manager);
    wlr_cursor_destroy(wimp.cursor);
    wl_display_destroy(wimp.display);
}


int main(int argc, char *argv[])
{
    int opt;
    int log_level = WLR_ERROR;
    wimp.config_file = NULL;

    while ((opt = getopt(argc, argv, "hc:di")) != -1) {
        switch (opt) {
	    case 'h':
		printf(usage);
		return EXIT_SUCCESS;
		break;
	    case 'c':
		wimp.config_file = strdup(optarg);
		break;
	    case 'd':
		log_level = WLR_DEBUG;
		break;
	    case 'i':
		log_level = WLR_INFO;
		break;
        }
    }

    init_log(log_level);

    // create
    wimp.display = wl_display_create();
    wimp.backend = wlr_backend_autocreate(wimp.display);
    wimp.renderer = wlr_backend_get_renderer(wimp.backend);
    wlr_renderer_init_wl_display(wimp.renderer, wimp.display);
    wlr_compositor_create(wimp.display, wimp.renderer);

    // initialise
    wl_list_init(&wimp.desks);
    wl_list_init(&wimp.key_bindings);
    wl_list_init(&wimp.mouse_bindings);
    wl_list_init(&wimp.marks);
    wl_list_init(&wimp.scratchpads);

    // configure
    locate_config();
    load_config();
    set_up_inputs();
    set_up_outputs();
    set_up_shell();
    set_up_cursor();
    set_up_decorations();
    set_up_layer_shell();

    // start
    const char *socket = wl_display_add_socket_auto(wimp.display);
    if (!socket || !wlr_backend_start(wimp.backend)) {
	shutdown();
	return EXIT_FAILURE;
    }
    setenv("WAYLAND_DISPLAY", socket, true);
    wlr_log(WLR_INFO, "Starting with WAYLAND_DISPLAY=%s", socket);
    schedule_auto_start();
    centre_cursor();
    wl_display_run(wimp.display);

    // stop
    wl_display_destroy_clients(wimp.display);
    shutdown();
    return EXIT_SUCCESS;
}
