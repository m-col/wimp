#include <stdlib.h>
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


struct server create_server() {
    struct server server;
    server.display = wl_display_create();
    server.backend = wlr_backend_autocreate(server.display, NULL);
    server.renderer = wlr_backend_get_renderer(server.backend);
    wlr_renderer_init_wl_display(server.renderer, server.display);
    wlr_compositor_create(server.display, server.renderer);
    wlr_data_device_manager_create(server.display);
    return server;
}


int main(int argc, char *argv[]) {
    wlr_log_init(WLR_ERROR, NULL);

    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
	    case 'd':
		wlr_log_init(WLR_DEBUG, NULL);
        }
    }

    // create
    struct server server = create_server();

    // read config
    load_config(&server);

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
