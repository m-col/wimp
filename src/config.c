#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <wayland-server-core.h>
#include <wlr/render/wlr_texture.h>
#include <unistd.h>

#include "action.h"
#include "config.h"
#include "desk.h"
#include "keybind.h"
#include "output.h"
#include "scratchpad.h"
#include "types.h"

#define CONFIG_HOME "$HOME/.config/wimp/startup"
#define CONFIG_HOME_XDG "$XDG_CONFIG_HOME/wimp/startup"


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


static void load_wallpaper(struct desk *desk, char *path) {
    cairo_surface_t *image = cairo_image_surface_create_from_png(path);

    if (!image || cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
	wlr_log(WLR_INFO, "Could not load image: %s", path);
	return;
    }

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
	wimp.renderer, WL_SHM_FORMAT_ARGB8888, stride, wallpaper->width,
	wallpaper->height, cairo_image_surface_get_data(canvas)
    );
    desk->wallpaper = wallpaper;
    damage_all_outputs();
    cairo_destroy(cr);
    cairo_surface_destroy(image);
    cairo_surface_destroy(canvas);
}


static void set_wallpaper(struct desk *desk, char *wallpaper) {
    // wallpaper is a colour
    if (strlen(wallpaper) == 7 && wallpaper[0] == '#') {
	assign_colour(wallpaper, desk->background);
	return;
    }

    // wallpaper is a path to an image
    wordexp_t p;
    wordexp(wallpaper, &p, WRDE_NOCMD | WRDE_UNDEF);
    load_wallpaper(desk, p.we_wordv[0]);
    wordfree(&p);
}


static void disable_vt_switching() {
    // TODO
}


static void setup_vt_switching() {
    uint32_t func_keys[] = {
	XKB_KEY_F1,
	XKB_KEY_F2,
	XKB_KEY_F3,
	XKB_KEY_F4,
	XKB_KEY_F5,
	XKB_KEY_F6,
    };
    struct binding *kb;

    for (unsigned i = 0; i < 6; i++) {
	kb = calloc(1, sizeof(struct binding));
	kb->mods = WLR_MODIFIER_CTRL;
	kb->key = func_keys[i];
	kb->action = &change_vt;
	kb->data = calloc(1, sizeof(unsigned));
	*(unsigned *)(kb->data) = i + 1;
	wl_list_insert(wimp.key_bindings.prev, &kb->link);
    }
}


void set_configurable(char *message, char *response) {
    char *s;
    if (!(s = strtok(NULL, " \t\n\r"))) {
	sprintf(response, "What do you want to set?.");
	return;
    }

    // desks <int>
    if (!strcasecmp(s, "desks")) {
	if ((s = strtok(NULL, " \t\n\r")) && is_number(s)) {
	    configure_desks(strtod(s, NULL));
	}
    }

    // desk ...
    else if (!strcasecmp(s, "desk")) {
	if ((s = strtok(NULL, " \t\n\r")) && is_number(s)) {
	    int index = strtod(s, NULL) - 1;
	    s = strtok(NULL, " \t\n\r");
	    struct desk *desk;
	    bool found = false;
	    wl_list_for_each(desk, &wimp.desks, link) {
		if (desk->index == index) {
		    found = true;
		    break;
		}
	    }
	    if (!found) {
		return;
	    }

	    // desk <index> background #rrggbb
	    // desk <index> background ~/path/to/image.png
	    if (!strcasecmp(s, "background")) {
		if ((s = strtok(NULL, " \t\n\r"))) {
		    set_wallpaper(desk, s);
		}
	    }

	    // desk <index> borders [focus|normal] <#rrggbb>
	    // desk <index> borders width <int>
	    else if (!strcasecmp(s, "borders")) {
		s = strtok(NULL, " \t\n\r");
		if (!strcasecmp(s, "normal")) {
		    assign_colour(strtok(NULL, " \t\n\r"), desk->border_normal);
		}
		else if (!strcasecmp(s, "focus")) {
		    assign_colour(strtok(NULL, " \t\n\r"), desk->border_focus);
		}
		else if (!strcasecmp(s, "width")) {
		    if ((s = strtok(NULL, " \t\n\r")) && is_number(s)) {
			desk->border_width = strtod(s, NULL);
		    }
		}
	    }

	    // desk <index> corners [focus|normal] <#rrggbb>
	    else if (!strcasecmp(s, "corners")) {
		s = strtok(NULL, " \t\n\r");
		if (!strcasecmp(s, "normal")) {
		    assign_colour(strtok(NULL, " \t\n\r"), desk->corner_normal);
		}
		else if (!strcasecmp(s, "focus")) {
		    assign_colour(strtok(NULL, " \t\n\r"), desk->corner_focus);
		}
	    }
	}
    }

    // zoom_min <float>
    else if (!strcasecmp(s, "zoom_min")) {
	if ((s = strtok(NULL, " \t\n\r")) && is_number(s)) {
	    wimp.zoom_min = strtod(s, NULL);
	}
    }

    // zoom_max <float>
    else if (!strcasecmp(s, "zoom_max")) {
	if ((s = strtok(NULL, " \t\n\r")) && is_number(s)) {
	    wimp.zoom_max = strtod(s, NULL);
	}
    }

    // mark_indicator #rrggbb
    else if (!strcasecmp(s, "mark_indicator")) {
	assign_colour(strtok(NULL, " \t\n\r"), wimp.mark_indicator.colour);
	wimp.mark_indicator.box.x = 0;
	wimp.mark_indicator.box.y = 0;
    }

    // snap_box #rrggbb
    else if (!strcasecmp(s, "snap_box")) {
	assign_colour(strtok(NULL, " \t\n\r"), wimp.snapbox_colour);
    }

    // vt_switching [off|on]
    else if (!strcasecmp(s, "vt_switching")) {
	s = strtok(NULL, " \t\n\r");
	if (!strcasecmp(s, "off"))
	    disable_vt_switching();
	else if (!strcasecmp(s, "on"))
	    setup_vt_switching();
    }

    // scroll_direction [natural|reverse]
    else if (!strcasecmp(s, "scroll_direction")) {
	s = strtok(NULL, " \t\n\r");
	if (!strcasecmp(s, "natural"))
	    wimp.reverse_scrolling = false;
	else if (!strcasecmp(s, "reverse"))
	    wimp.reverse_scrolling = true;
    }

    // e.g.: set_modifier logo
    else if (!strcasecmp(s, "mod")) {
	set_mod(message, response);
    }

    // auto_focus [on|off]
    else if (!strcasecmp(s, "auto_focus")) {
	s = strtok(NULL, " \t\n\r");
	if (!strcasecmp(s, "off")) {
	    wimp.auto_focus = false;
	} else if (!strcasecmp(s, "on")) {
	    wimp.auto_focus = true;
	}
    }
}


struct startup_data {
    struct wl_event_source *wl_event_source;
    char *script;
};


static int _startup(void *vdata) {
    struct startup_data *data = vdata;
    wl_event_source_remove(data->wl_event_source);
    if (fork() == 0) {
	if (execl(data->script, data->script, (void *)NULL) == -1) {
	    exit(EXIT_FAILURE);
	}
    } else {
	wlr_log(WLR_DEBUG, "Running startup script: %s", data->script);
    }
    free(data->script);
    free(data);
    return 0;
}


void schedule_startup() {
    char *xdg_config = getenv("XDG_CONFIG_HOME");
    char *config_dir = (xdg_config && *xdg_config) ? CONFIG_HOME_XDG : CONFIG_HOME;
    struct startup_data *data = calloc(1, sizeof(struct startup_data));
    wordexp_t p;
    wordexp(config_dir, &p, WRDE_NOCMD | WRDE_UNDEF);
    data->script = strdup(p.we_wordv[0]);
    wordfree(&p);

    if (is_executable(data->script)) {
	struct wl_event_loop *event_loop = wl_display_get_event_loop(wimp.display);
	data->wl_event_source = wl_event_loop_add_timer(event_loop, _startup, data);
	wl_event_source_timer_update(data->wl_event_source, 1);
    } else {
	wlr_log(WLR_ERROR, "Cannot run startup script: %s", data->script);
	free(data->script);
	free(data);
    }

}
