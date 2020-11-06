#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "types.h"


void assign_colour(char *hex, float dest[4]) {
    switch (strlen(hex)) {
	case 6:
	case 8:
	    break;
	case 7:
	case 9:
	    hex++;
	    break;
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


void set_defaults(struct server *server) {
    assign_colour("#5D479D", &server->background);
}


void load_config(struct server *server) {
    set_defaults(server);

    char config[1024] = "";
    strcpy(config, getenv("HOME"));
    strcat(config, "/.config/deskwm.conf");

    if(access(config, R_OK) == -1) {
	return;
    }
    FILE *fd = fopen(config, "r");

    char *s;
    char linebuf[1024];
    while (fgets(linebuf, sizeof(linebuf), fd)) {
	if (!(s = strtok(linebuf, " \t\n\r")) || s[0] == '#') {
	    continue;
	}
	if (!strcasecmp(s, "background")) {
	    if ((s = strtok(NULL, " \t\n\r"))) {
		assign_colour(s, &server->background);
	    }
	} else if (!strcasecmp(s, "modifier")) {
	}
    }
    fclose(fd);
}
