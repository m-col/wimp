#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <wlr/render/wlr_texture.h>
#include <unistd.h>

#include "action.h"
#include "config.h"
#include "desk.h"
#include "input.h"
#include "types.h"

#define is_decimal(s) (strspn(s, "0123456789.") == strlen(s))


enum wlr_keyboard_modifier modifier_by_name(char *mod) {
    if (strcasecmp(mod, "shift") == 0)
	return WLR_MODIFIER_SHIFT;
    if (strcasecmp(mod, "caps") == 0)
	return WLR_MODIFIER_CAPS;
    if (strcasecmp(mod, "ctrl") == 0)
	return WLR_MODIFIER_CTRL;
    if (strcasecmp(mod, "alt") == 0)
	return WLR_MODIFIER_ALT;
    if (strcasecmp(mod, "mod2") == 0)
	return WLR_MODIFIER_MOD2;
    if (strcasecmp(mod, "mod3") == 0)
	return WLR_MODIFIER_MOD3;
    if (strcasecmp(mod, "logo") == 0)
	return WLR_MODIFIER_LOGO;
    if (strcasecmp(mod, "mod5") == 0)
	return WLR_MODIFIER_MOD5;
    return 0;
}


void assign_action(char *name, char *data, struct binding *kb) {
    kb->data = NULL;
    kb->action = NULL;

    if (strcasecmp(name, "shutdown") == 0)
	kb->action = &shutdown;
    else if (strcasecmp(name, "close_current_window") == 0)
	kb->action = &close_current_window;
    else if (strcasecmp(name, "next_desk") == 0)
	kb->action = &next_desk;
    else if (strcasecmp(name, "prev_desk") == 0)
	kb->action = &prev_desk;
    else if (strcasecmp(name, "pan_desk") == 0)
	kb->action = &pan_desk;
    else if (strcasecmp(name, "reset_pan") == 0)
	kb->action = &reset_pan;
    else if (strcasecmp(name, "save_pan") == 0)
	kb->action = &save_pan;
    else if (strcasecmp(name, "zoom_in") == 0) {
	kb->action = &zoom_desk;
	kb->data = calloc(1, sizeof(int));
	*(int *)(kb->data) = 1;
    }
    else if (strcasecmp(name, "zoom_out") == 0) {
	kb->action = &zoom_desk;
	kb->data = calloc(1, sizeof(int));
	*(int *)(kb->data) = -1;
    }
    else if (strcasecmp(name, "zoom_out") == 0) {
	kb->action = &zoom_desk;
	kb->data = calloc(1, sizeof(int));
	*(int *)(kb->data) = -1;
    }
    else if (strcasecmp(name, "exec") == 0) {
	kb->action = &exec_command;
	kb->data = calloc(strlen(data), sizeof(char));
	strncpy(kb->data, data, strlen(data));
    }
}


enum mouse_keys mouse_key_from_name(char *name) {
    if (strcasecmp(name, "motion") == 0)
	return MOTION;
    else if (strcasecmp(name, "scroll") == 0)
	return SCROLL;
    return 0;
}


void add_binding(struct server *server, char *data, int line) {
    struct binding *kb = calloc(1, sizeof(struct binding));
    enum wlr_keyboard_modifier mod;
    char *s = strtok(data, " \t\n\r");
    bool is_mouse_binding = false;

    // additional modifiers
    kb->mods = 0;
    for (int i = 0; i < 8; i++) {
	mod = modifier_by_name(s);
	if (mod == 0)
	    break;
	kb->mods |= mod;
	s = strtok(NULL, " \t\n\r");
    }

    // key
    kb->key = xkb_keysym_from_name(s, XKB_KEYSYM_CASE_INSENSITIVE);
    if (kb->key == XKB_KEY_NoSymbol) {
	kb->key = mouse_key_from_name(s);
	if (kb->key == 0) {
	    wlr_log(WLR_ERROR, "Config line %i: No such key '%s'.", line, s);
	    free(kb);
	    return;
	}
	is_mouse_binding = true;
    }

    // action
    s = strtok(NULL, " \t\n\r");
    assign_action(s, strtok(NULL, ""), kb);
    if (kb->action == NULL) {
	wlr_log(WLR_ERROR, "Config line %i: No such action '%s'.", line, s);
	free(kb);
	return;
    }

    struct binding *kb_existing;
    struct wl_list *bindings;
    if (is_mouse_binding) {
	bindings = &server->mouse_bindings;
    } else {
	bindings = &server->key_bindings;
    }
    wl_list_for_each(kb_existing, bindings, link) {
	if (kb_existing->key == kb->key && kb_existing->mods == kb->mods) {
	    wlr_log(WLR_ERROR, "Config line %i: binding exists for key combo.", line);
	    free(kb);
	    return;
	}
    }
    wl_list_insert(bindings->prev, &kb->link);
}


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

    server->mod = WLR_MODIFIER_LOGO;
    wl_list_init(&server->key_bindings);
    wl_list_init(&server->mouse_bindings);
    server->on_mouse_motion = NULL;
    server->on_mouse_scroll = NULL;
    server->zoom_min = 0.5;
    server->zoom_max = 3;
    server->reverse_scrolling = false;
    server->vt_switching = true;
}


static void setup_vt_switching(struct server *server) {
    if (!server->vt_switching)
	return;

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
	wl_list_insert(server->key_bindings.prev, &kb->link);
    }
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
	wlr_log(WLR_ERROR, "%s not accessible. Using defaults.", p.we_wordv[0]);
	wordfree(&p);
	return;
    }
    FILE *fd = fopen(p.we_wordv[0], "r");
    wordfree(&p);

    char *s;
    char linebuf[1024];
    int line = 0;
    while (fgets(linebuf, sizeof(linebuf), fd)) {
	line++;
	if (!(s = strtok(linebuf, " \t\n\r")) || s[0] == '#') {
	    continue;
	}
	if (!strcasecmp(s, "add_desk")) {
	    add_desk(server);
	} else if (!strcasecmp(s, "background")) {
	    if ((s = strtok(NULL, " \t\n\r"))) {
		set_wallpaper(server, s);
	    }
	} else if (!strcasecmp(s, "set_modifier")) {
	    if ((s = strtok(NULL, " \t\n\r"))) {
		int mod = modifier_by_name(s);
		if (mod)
		    server->mod = mod;
		else
		    wlr_log(WLR_ERROR, "Config line %i: invalid modifier name '%s'.", line, s);
	    }
	} else if (!strcasecmp(s, "zoom_min")) {
	    if ((s = strtok(NULL, " \t\n\r")) && is_decimal(s)) {
		server->zoom_min = strtod(s, NULL);
	    }
	} else if (!strcasecmp(s, "zoom_max")) {
	    if ((s = strtok(NULL, " \t\n\r")) && is_decimal(s)) {
		server->zoom_max = strtod(s, NULL);
	    }
	} else if (!strcasecmp(s, "bind")) {
	    add_binding(server, strtok(NULL, ""), line);
	} else if (!strcasecmp(s, "vt_switching")) {
	    s = strtok(NULL, " \t\n\r");
	    if (!strcasecmp(s, "off"))
		server->vt_switching = false;
	}
    }
    fclose(fd);

    setup_vt_switching(server);
}
