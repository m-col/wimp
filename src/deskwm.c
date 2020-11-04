#include <stdlib.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "deskwm.h"
#include "output.h"
#include "shell.h"


struct server create_server() {
    struct server server;
    server.display = wl_display_create();
    server.backend = wlr_backend_autocreate(server.display, NULL);
    server.renderer = wlr_backend_get_renderer(server.backend);
    wlr_renderer_init_wl_display(server.renderer, server.display);
    wlr_compositor_create(server.display, server.renderer);
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

    // configure
    set_up_outputs(&server);
    set_up_shell(&server);

    // start
    wl_display_run(server.display);

    // stop
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.display);
    return 0;
}
