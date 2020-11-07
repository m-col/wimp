#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <unistd.h>

#include "config.h"
#include "shell.h"
#include "types.h"


void assign_colour(char *hex, float dest[4]) {
    switch (strlen(hex)) {
	case 6:
	case 8:
	    break;
	case 7:
	case 9:
	    if (hex[0] == '#') {
		hex++;
		break;
	    }
	default:
	    return;
    }

    char c[2];
    for (int i = 0; i < 3; i++) {
	strncpy(c, &hex[i * 2], 2);
	dest[i] = (float)strtol(c, NULL, 16) / 255;
    }
    dest[4] = 1;
}


static void set_defaults(struct server *server) {
    add_desk(server);
    server->current_desk =
	wl_container_of(server->desks.prev, server->current_desk, link);
}


void load_config(struct server *server, char *config) {
    set_defaults(server);

    if (config == NULL) {
	char *xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home && *xdg_config_home) {
	    config = "$XDG_CONFIG_HOME/.config/deskwm.conf";
	} else {
	    config = "$HOME/.config/deskwm.conf";
	}
    };
    wordexp_t p;
    wordexp(config, &p, WRDE_NOCMD | WRDE_UNDEF);

    if(access(p.we_wordv[0], R_OK) == -1) {
	wlr_log(WLR_INFO, "%s not accessible. Using defaults.", p.we_wordv[0]);
	wordfree(&p);
	return;
    }
    FILE *fd = fopen(p.we_wordv[0], "r");
    wordfree(&p);

    char *s;
    char linebuf[1024];
    while (fgets(linebuf, sizeof(linebuf), fd)) {
	if (!(s = strtok(linebuf, " \t\n\r")) || s[0] == '#') {
	    continue;
	}
	if (!strcasecmp(s, "add_desk")) {
	    add_desk(server);
	} else if (!strcasecmp(s, "background")) {
	    if ((s = strtok(NULL, " \t\n\r"))) {
		struct desk *desk = wl_container_of(server->desks.prev, desk, link);
		assign_colour(s, desk->background);
	    }
	} else if (!strcasecmp(s, "modifier")) {
	}
    }
    fclose(fd);
}
