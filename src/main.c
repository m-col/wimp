#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "cursor.h"
#include "decorations.h"
#include "desk.h"
#include "main.h"
#include "keyboard.h"
#include "layer_shell.h"
#include "log.h"
#include "output.h"
#include "shell.h"
#include "types.h"


struct wimp wimp = {
    .mark_waiting = false,
    .on_mouse_motion = NULL,
    .on_mouse_scroll = NULL,
    .can_steal_focus = true,
    .desk_count = 0,
    .mark_indicator.box.width = 25,
    .mark_indicator.box.height = 25,
    .mark_indicator.box.x = 0,
    .mark_indicator.box.y = 0,
};


static const char usage[] =
    "usage: wimp [options...]\n"
    "    -h         show this help message\n"
    "    -c <file>  specify config file\n"
    "    -d         set logging to debug mode\n"
    "    -i         set logging to info mode\n"
;


static void free_stuff() {
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
	    free(view);
	};
	wl_list_remove(&desk->link);
	free(desk->wallpaper);
	free(desk);
    };

    struct output *output, *toutput;
    struct layer_view *lview, *tlview;
    wl_list_for_each_safe(output, toutput, &wimp.outputs, link) {
	wl_list_for_each_safe(lview, tlview, &output->layer_views, link) {
	    wl_list_remove(&lview->link);
	    free(lview);
	};
	wl_list_remove(&output->link);
	free(output);
    };

    struct keyboard *keyboard, *tkeyboard;
    wl_list_for_each_safe(keyboard, tkeyboard, &wimp.keyboards, link) {
	wl_list_remove(&keyboard->link);
	free(keyboard);
    };

    struct mark *mark, *tmark;
    wl_list_for_each_safe(mark, tmark, &wimp.marks, link) {
	wl_list_remove(&mark->link);
	free(mark);
    };
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
    wimp.backend = wlr_backend_autocreate(wimp.display, NULL);
    wimp.renderer = wlr_backend_get_renderer(wimp.backend);
    wlr_renderer_init_wl_display(wimp.renderer, wimp.display);
    wlr_compositor_create(wimp.display, wimp.renderer);

    // add some managers
    wlr_screencopy_manager_v1_create(wimp.display);
    wlr_data_device_manager_create(wimp.display);
    wlr_primary_selection_v1_device_manager_create(wimp.display);

    // initialise
    wl_list_init(&wimp.desks);
    wl_list_init(&wimp.key_bindings);
    wl_list_init(&wimp.mouse_bindings);
    wl_list_init(&wimp.marks);
    add_desk();
    wimp.current_desk = wl_container_of(wimp.desks.next, wimp.current_desk, link);

    // configure
    locate_config();
    load_config();
    set_up_outputs();
    set_up_shell();
    set_up_cursor();
    set_up_keyboard();
    set_up_decorations();
    set_up_layer_shell();

    // start
    const char *socket = wl_display_add_socket_auto(wimp.display);
    if (!socket) {
	wlr_backend_destroy(wimp.backend);
	wl_display_destroy(wimp.display);
	return EXIT_FAILURE;
    }
    if (!wlr_backend_start(wimp.backend)) {
	wlr_backend_destroy(wimp.backend);
	wl_display_destroy(wimp.display);
	return 1;
    }
    setenv("WAYLAND_DISPLAY", socket, true);
    wlr_log(WLR_INFO, "Starting with WAYLAND_DISPLAY=%s", socket);
    wl_display_run(wimp.display);

    // stop
    wl_display_destroy_clients(wimp.display);
    free_stuff();
    wlr_backend_destroy(wimp.backend);
    wl_display_destroy(wimp.display);
    return EXIT_SUCCESS;
}
