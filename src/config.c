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
#include "types.h"

#define is_number(s) (strspn(s, "0123456789.-") == strlen(s))


static const char *DEFAULT_CONFIG =  "\n\
zoom_min 0.5\n\
zoom_max 3\n\
mark_indicator #000000\n\
vt_switching on\n\
set_modifier Logo\n\
bind Ctrl Escape shutdown\n\
scroll_direction natural\n\
";


struct value_map {
    const char *name;
    int value;
};


static struct value_map mods[] = {
    { "shift", WLR_MODIFIER_SHIFT },
    { "caps", WLR_MODIFIER_CAPS },
    { "ctrl", WLR_MODIFIER_CTRL },
    { "alt", WLR_MODIFIER_ALT },
    { "mod2", WLR_MODIFIER_MOD2 },
    { "mod3", WLR_MODIFIER_MOD3 },
    { "logo", WLR_MODIFIER_LOGO },
    { "mod5", WLR_MODIFIER_MOD5 },
};


static struct value_map dirs[] = {
    { "up", UP },
    { "right", RIGHT },
    { "down", DOWN },
    { "left", LEFT },
};


static struct value_map mouse_keys[] = {
    { "motion", MOTION },
    { "scroll", SCROLL },
};


/* To get the length of these arrays we need to calculate that before passing
 * them to the getter so the macro hides this. */
#define get(arr, name) _get(arr, sizeof(arr) / sizeof(arr[0]), name)

int _get(struct value_map *values, int len, const char *name) {
    for (int i = 0; i < len; i++) {
	if (strcasecmp(values[i].name, name) == 0)
	    return values[i].value;
    }
    return 0;
}


void dir_handler(struct binding *kb, char *data) {
    kb->data = calloc(1, sizeof(enum direction));
    *(enum direction *)(kb->data) = get(dirs, data);
}


void str_handler(struct binding *kb, char *data) {
    if (is_number(data)) {
	kb->data = calloc(1, sizeof(double));
	*(double *)(kb->data) = strtod(data, NULL);
    } else {
	kb->data = calloc(strlen(data), sizeof(char));
	strncpy(kb->data, data, strlen(data));
    }
}


static struct {
    const char *name;
    action action;
    void (*data_handler)(struct binding *kb, char *data);
} action_map[] = {
    { "shutdown", &shutdown, NULL },
    { "exec", &exec_command, &str_handler },
    { "close_window", &close_window, NULL },
    { "focus", &focus_in_direction, &dir_handler },
    { "next_desk", &next_desk, NULL },
    { "prev_desk", &prev_desk, NULL },
    { "pan_desk", &pan_desk, NULL },
    { "reset_zoom", &reset_zoom, NULL },
    { "zoom", &zoom, &str_handler },
    { "zoom_mouse", &zoom_mouse, NULL },
    { "set_mark", &set_mark, NULL },
    { "go_to_mark", &go_to_mark, NULL },
    { "toggle_fullscreen", &toggle_fullscreen, NULL },
    { "halfimize", &halfimize, NULL },
    { "reload_config", &reload_config, NULL },
};

static int num_actions = sizeof(action_map) / sizeof(action_map[0]);


bool assign_action(char *name, char *data, struct binding *kb) {
    kb->action = NULL;
    kb->data = NULL;

    for (int i = 0; i < num_actions; i++) {
	if (strcmp(action_map[i].name, name) == 0) {
	    kb->action = action_map[i].action;
	    if (action_map[i].data_handler != NULL)
		action_map[i].data_handler(kb, data);
	    return true;
	}
    }
    return false;
}


void free_binding(struct binding *kb) {
    if (kb->data)
	free(kb->data);
    wl_list_remove(&kb->link);
    free(kb);
}


void add_binding(struct server *server, char *data, int line) {
    struct binding *kb = calloc(1, sizeof(struct binding));
    enum wlr_keyboard_modifier mod;
    char *s = strtok(data, " \t\n\r");
    bool is_mouse_binding = false;

    // additional modifiers
    kb->mods = 0;
    for (int i = 0; i < 8; i++) {
	mod = get(mods, s);
	if (mod == 0)
	    break;
	kb->mods |= mod;
	s = strtok(NULL, " \t\n\r");
    }

    // key
    kb->key = xkb_keysym_from_name(s, XKB_KEYSYM_NO_FLAGS);
    if (kb->key == XKB_KEY_NoSymbol)
	kb->key = xkb_keysym_from_name(s, XKB_KEYSYM_CASE_INSENSITIVE);

    if (kb->key == XKB_KEY_NoSymbol) {
	kb->key = get(mouse_keys, s);
	if (kb->key == 0) {
	    wlr_log(WLR_ERROR, "Config line %i: No such key '%s'.", line, s);
	    free(kb);
	    return;
	}
	is_mouse_binding = true;
    }

    // action
    s = strtok(NULL, " \t\n\r");
    if (!assign_action(s, strtok(NULL, "\n\r"), kb)) {
	wlr_log(WLR_ERROR, "Config line %i: No such action '%s'.", line, s);
	free(kb);
	return;
    }

    struct binding *kb_existing, *tmp;
    struct wl_list *bindings;
    if (is_mouse_binding) {
	bindings = &server->mouse_bindings;
    } else {
	bindings = &server->key_bindings;
    }
    wl_list_for_each_safe(kb_existing, tmp, bindings, link) {
	if (kb_existing->key == kb->key && kb_existing->mods == kb->mods) {
	    free_binding(kb_existing);
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
	server->renderer, WL_SHM_FORMAT_ARGB8888, stride, wallpaper->width,
	wallpaper->height, cairo_image_surface_get_data(canvas)
    );
    desk->wallpaper = wallpaper;
    cairo_destroy(cr);
    cairo_surface_destroy(image);
    cairo_surface_destroy(canvas);
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


static void setup_vt_switching(struct server *server) {
    if (!server->vt_switching)
	return;

    /* If the configured mod key is alt then we have to bind the VT key combos
     * otherwise they interfere with the alt. With other mods we don't get this
     * key combo so we have to bind the function keys manually. */
    uint32_t func_keys[] = {
	XKB_KEY_F1,
	XKB_KEY_F2,
	XKB_KEY_F3,
	XKB_KEY_F4,
	XKB_KEY_F5,
	XKB_KEY_F6,
    };
    uint32_t switch_keys[] = {
	XKB_KEY_XF86Switch_VT_1,
	XKB_KEY_XF86Switch_VT_2,
	XKB_KEY_XF86Switch_VT_3,
	XKB_KEY_XF86Switch_VT_4,
	XKB_KEY_XF86Switch_VT_5,
	XKB_KEY_XF86Switch_VT_6
    };
    uint32_t *keys;
    if (server->mod == WLR_MODIFIER_ALT) {
	keys = switch_keys;
    } else {
	keys = func_keys;
    }

    struct binding *kb;

    for (unsigned i = 0; i < 6; i++) {
	kb = calloc(1, sizeof(struct binding));
	kb->mods = WLR_MODIFIER_CTRL;
	kb->key = *keys;
	keys++;
	kb->action = &change_vt;
	kb->data = calloc(1, sizeof(unsigned));
	*(unsigned *)(kb->data) = i + 1;
	wl_list_insert(server->key_bindings.prev, &kb->link);
    }
}


void parse_config(struct server *server, FILE *stream) {
    char *s;
    char linebuf[1024];
    int line = 0;

    while (fgets(linebuf, sizeof(linebuf), stream)) {
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
		enum wlr_keyboard_modifier mod = get(mods, s);
		if (mod)
		    server->mod = mod;
		else
		    wlr_log(WLR_ERROR, "Config line %i: invalid modifier name '%s'.", line, s);
	    }
	} else if (!strcasecmp(s, "zoom_min")) {
	    if ((s = strtok(NULL, " \t\n\r")) && is_number(s)) {
		server->zoom_min = strtod(s, NULL);
	    }
	} else if (!strcasecmp(s, "zoom_max")) {
	    if ((s = strtok(NULL, " \t\n\r")) && is_number(s)) {
		server->zoom_max = strtod(s, NULL);
	    }
	} else if (!strcasecmp(s, "bind")) {
	    add_binding(server, strtok(NULL, ""), line);
	} else if (!strcasecmp(s, "mark_indicator")) {
	    assign_colour(strtok(NULL, " \t\n\r"), server->mark_indicator.colour);
	} else if (!strcasecmp(s, "vt_switching")) {
	    s = strtok(NULL, " \t\n\r");
	    if (!strcasecmp(s, "off"))
		server->vt_switching = false;
	    else if (!strcasecmp(s, "on"))
		server->vt_switching = true;
	} else if (!strcasecmp(s, "scroll_direction")) {
	    s = strtok(NULL, " \t\n\r");
	    if (!strcasecmp(s, "natural"))
		server->reverse_scrolling = false;
	    else if (!strcasecmp(s, "reverse"))
		server->reverse_scrolling = true;
	}
    }
}


void load_config(struct server *server) {
    FILE *stream;

    // remove existing keybindings
    struct binding *kb, *tmp;
    wl_list_for_each_safe(kb, tmp, &server->mouse_bindings, link) {
	free_binding(kb);
    }
    wl_list_for_each_safe(kb, tmp, &server->key_bindings, link) {
	free_binding(kb);
    }

    // load defaults
    char *buffer = NULL;
    size_t size = 0;
    stream = open_memstream(&buffer, &size);
    fprintf(stream, DEFAULT_CONFIG);
    rewind(stream);
    parse_config(server, stream);
    free(buffer);
    fclose(stream);

    // load custom configuration file
    if (server->config_file) {
	stream = fopen(server->config_file, "r");
	parse_config(server, stream);
	fclose(stream);
    }

    setup_vt_switching(server);
}


void locate_config(struct server *server)
{
    char *config;
    char *xdg_config;

    if (server->config_file == NULL) {
	xdg_config = getenv("XDG_CONFIG_HOME");
	config = (xdg_config && *xdg_config) ?
	    "$XDG_CONFIG_HOME/.config/deskwm.conf" : "$HOME/.config/deskwm.conf";
	server->config_file = strdup(config);
    };

    wordexp_t p;
    wordexp(server->config_file, &p, WRDE_NOCMD | WRDE_UNDEF);
    free(server->config_file);

    if (access(p.we_wordv[0], R_OK) == -1) {
	wlr_log(WLR_ERROR, "%s not accessible. Using defaults.", p.we_wordv[0]);
	server->config_file = NULL;
    } else {
	server->config_file = strdup(p.we_wordv[0]);
    }

    wordfree(&p);
}
