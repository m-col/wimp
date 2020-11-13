#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <wlr/render/wlr_texture.h>
#include <unistd.h>

#include "config.h"
#include "desk.h"
#include "types.h"

#define is_decimal(s) (strspn(s, "0123456789.") == strlen(s))


void assign_colour(char *hex, float dest[4]) {
    if (strlen(hex) != 7 || hex[0] != '#') {
	wlr_log(WLR_INFO, "Invalid colour: %s. Should be in the form #rrggbb.", hex);
	return;
    }
    hex++;

    char c[2];
    for (int i = 0; i < 3; i++) {
	strncpy(c, &hex[i * 2], 2);
	dest[i] = (float)strtol(c, NULL, 16) / 255;
    }
    dest[3] = 1;
}


static void load_wallpaper(
    struct server *server, struct desk *desk, char *path
) {
    cairo_surface_t *image = cairo_image_surface_create_from_png(path);

    if (!image)
	goto fail;
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS)
	goto fail;

    struct wallpaper *wallpaper = calloc(1, sizeof(struct wallpaper));
    wallpaper->width = cairo_image_surface_get_width(image);
    wallpaper->height = cairo_image_surface_get_height(image);
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, wallpaper->width);

    // we copy the image to a second surface to ensure the pixel format is
    // compatible with wlr_texture_from_pixels
    cairo_surface_t *canvas = cairo_image_surface_create(
	CAIRO_FORMAT_ARGB32, wallpaper->width, wallpaper->height);
    cairo_t *cr = cairo_create(canvas);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_paint(cr);

    wallpaper->texture = wlr_texture_from_pixels(
	server->renderer, WL_SHM_FORMAT_ARGB8888, stride, wallpaper->width,
	wallpaper->height, cairo_image_surface_get_data(canvas)
    );
    desk->wallpaper = wallpaper;
    cairo_destroy(cr);
    cairo_surface_destroy(image);
    cairo_surface_destroy(canvas);
    return;

fail:
    wlr_log(WLR_INFO, "Could not load image: %s", path);
}


static void set_wallpaper(struct server *server, char *wallpaper) {
    struct desk *desk = wl_container_of(server->desks.prev, desk, link);

    // wallpaper is a colour
    if (strlen(wallpaper) == 7 && wallpaper[0] == '#') {
	assign_colour(wallpaper, desk->background);
	return;
    }

    // wallpaper is a path to an image
    wordexp_t p;
    wordexp(wallpaper, &p, WRDE_NOCMD | WRDE_UNDEF);
    load_wallpaper(server, desk, p.we_wordv[0]);
    wordfree(&p);
}


static void set_defaults(struct server *server) {
    add_desk(server);
    server->current_desk =
	wl_container_of(server->desks.next, server->current_desk, link);
    server->zoom_min = 0.5;
    server->zoom_max = 3;
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

    if (access(p.we_wordv[0], R_OK) == -1) {
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
		set_wallpaper(server, s);
	    }
	} else if (!strcasecmp(s, "zoom_min")) {
	    if ((s = strtok(NULL, " \t\n\r")) && is_decimal(s)) {
		server->zoom_min = strtod(s, NULL);
	    }
	} else if (!strcasecmp(s, "zoom_max")) {
	    if ((s = strtok(NULL, " \t\n\r")) && is_decimal(s)) {
		server->zoom_max = strtod(s, NULL);
	    }
	}
    }
    fclose(fd);
}
