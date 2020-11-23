#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "deskwm.h"
#include "input.h"
#include "output.h"
#include "shell.h"
#include "types.h"


static const char usage[] =
    "usage: deskwm [options...]\n"
    "    -h         show this help message\n"
    "    -c <file>  specify config file\n"
    "    -d         set logging to debug mode\n"
    "    -i         set logging to info mode\n"
;


int main(int argc, char *argv[]) {
    wlr_log_init(WLR_ERROR, NULL);

    int opt;
    char *config = NULL;
    while ((opt = getopt(argc, argv, "hcdi:")) != -1) {
        switch (opt) {
	    case 'h':
		printf(usage);
		return EXIT_SUCCESS;
		break;
	    case 'c':
		config = strdup(optarg);
		break;
	    case 'd':
		wlr_log_init(WLR_DEBUG, NULL);
		break;
	    case 'i':
		wlr_log_init(WLR_INFO, NULL);
		break;
        }
    }

    // create
    struct server server;
    server.display = wl_display_create();
    server.backend = wlr_backend_autocreate(server.display, NULL);
    server.renderer = wlr_backend_get_renderer(server.backend);
    wlr_renderer_init_wl_display(server.renderer, server.display);
    wlr_compositor_create(server.display, server.renderer);
    wlr_data_device_manager_create(server.display);
    wl_list_init(&server.desks);
    server.can_steal_focus = false;

    // read config
    load_config(&server, config);
    free(config);

    // configure
    set_up_outputs(&server);
    set_up_shell(&server);
    set_up_cursor(&server);
    set_up_keyboard(&server);

    // start
    const char *socket = wl_display_add_socket_auto(server.display);
    if (!socket) {
	wlr_backend_destroy(server.backend);
	wl_display_destroy(server.display);
	return EXIT_FAILURE;
    }
    if (!wlr_backend_start(server.backend)) {
	wlr_backend_destroy(server.backend);
	wl_display_destroy(server.display);
	return 1;
    }
    setenv("WAYLAND_DISPLAY", socket, true);
    wlr_log(WLR_INFO, "Starting with WAYLAND_DISPLAY=%s", socket);
    wl_display_run(server.display);

    // stop
    wl_display_destroy_clients(server.display);
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.display);
    return EXIT_SUCCESS;
}
