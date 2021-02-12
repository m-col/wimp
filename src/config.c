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
#include "output.h"
#include "scratchpad.h"
#include "types.h"

#define is_number(s) (strspn(s, "0123456789.-") == strlen(s))
#define is_number_perc(s) (strspn(s, "0123456789.-%") == strlen(s))
#define CONFIG_HOME "$HOME/.config/wimp/"
#define CONFIG_HOME_XDG "$XDG_CONFIG_HOME/wimp/"


static const char *DEFAULT_CONFIG =  "\n\
desks 2\n\
desk 1 background #5D479D\n\
desk 1 borders normal #3e3e73\n\
desk 1 borders focus #998dd1\n\
desk 1 borders width 4\n\
desk 1 corners resize #406794\n\
desk 2 background #3e3e73\n\
desk 2 borders normal #5D479D\n\
desk 2 borders focus #998dd1\n\
desk 2 borders width 6\n\
desk 2 corners resize #3e3e73\n\
zoom_min 0.2\n\
zoom_max 5\n\
mark_indicator #000000\n\
vt_switching on\n\
scroll_direction natural\n\
set_modifier Logo\n\
auto_focus on\n\
bind Ctrl Escape shutdown\n\
";


static char *expand(char *path) {
    wordexp_t p;
    wordexp(path, &p, WRDE_NOCMD | WRDE_UNDEF);
    free(path);
    char *new = strdup(p.we_wordv[0]);
    wordfree(&p);
    return new;
}


struct value_map {
    const char *name;
    const int value;
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
    { "drag1", DRAG1 },
    { "drag2", DRAG2 },
    { "drag3", DRAG3 },
    { "pinch", PINCH },
};


/* To get the length of these arrays we need to calculate that before passing
 * them to the getter so the macro hides this. */
#define get(arr, name) _get(arr, sizeof(arr) / sizeof(arr[0]), name)

static int _get(struct value_map *values, const int len, const char *name) {
    for (int i = 0; i < len; i++) {
	if (strcasecmp(values[i].name, name) == 0)
	    return values[i].value;
    }
    return 0;
}


static bool dir_handler(struct binding *kb, char *data, int line) {
    kb->data = calloc(1, sizeof(enum direction));
    *(enum direction *)(kb->data) = get(dirs, data);
    return true;
}


static bool str_handler(struct binding *kb, char *data, int line) {
    if (is_number(data)) {
	kb->data = calloc(1, sizeof(double));
	*(double *)(kb->data) = strtod(data, NULL);
    } else {
	kb->data = calloc(strlen(data), sizeof(char));
	strncpy(kb->data, data, strlen(data));
    }
    return true;
}


static bool motion_handler(struct binding *kb, char *data, int line) {
    char *s;
    if (!data) {
	goto err;
    }

    s = strtok(data, " \t\n\r");
    if (!is_number(s) || !data) {
	goto err;
    }
    int x = atoi(s);

    s = strtok(NULL, " \t\n\r");
    if (!is_number(s)) {
	goto err;
    }
    int y = atoi(s);

    struct motion motion = {
	.dx = x,
	.dy = y,
	.is_percentage = true,
    };
    kb->data = calloc(1, sizeof(struct motion));
    *(struct motion *)(kb->data) = motion;
    return true;

err:
    wlr_log(WLR_ERROR, "Config line %i malformed/incomplete.", line);
    return false;
}


static bool wlr_box_from_str(char* str, struct wlr_box *box) {
    // turns e.g. 1920x1800+500+500 into a wlr_box
    char *w, *h, *x, *y;
    if (str) {
	if ((w = strtok(str, "x")) && is_number_perc(w)) {
	    if ((h = strtok(NULL, "+")) && is_number_perc(h)) {
		if ((x = strtok(NULL, "+")) && is_number_perc(x)) {
		    if ((y = strtok(NULL, " \t\n\r")) && is_number_perc(y)) {
			if (is_number(w)) {
			    box->width = atoi(w);
			} else {
			    if ((w = strtok(w, "%"))) {
				box->width = - atoi(w);
			    } else {
				return false;
			    }
			}
			if (is_number(h)) {
			    box->height = atoi(h);
			} else {
			    if ((h = strtok(h, "%"))) {
				box->height = - atoi(h);
			    } else {
				return false;
			    }
			}
			if (is_number(x)) {
			    box->x = atoi(x);
			} else {
			    if ((x = strtok(x, "%"))) {
				box->x = - atoi(x);
			    } else {
				return false;
			    }
			}
			if (is_number(y)) {
			    box->y = atoi(y);
			} else {
			    if ((y = strtok(y, "%"))) {
				box->y = - atoi(y);
			    } else {
				return false;
			    }
			}
			return true;
		    }
		}
	    }
	}
    }
    return false;
}


static int scratchpad_id = 0;


static bool scratchpad_handler(struct binding *kb, char *data, int line) {
    char *geo, *command;
    struct wlr_box box;
    if (!data) {
	goto err;
    }
    if (!(geo = strtok(data, " \t\n\r"))) {
	goto err;
    }
    if (!(command = strtok(NULL, "\n\r"))) {
	goto err;
    }
    if (!wlr_box_from_str(geo, &box)) {
	goto err;
    }

    struct scratchpad *scratchpad = calloc(1, sizeof(struct scratchpad));
    wl_list_insert(wimp.scratchpads.prev, &scratchpad->link);
    scratchpad->command = strdup(command);
    scratchpad->id = scratchpad_id;
    scratchpad->is_mapped = false;
    scratchpad->view = NULL;
    scratchpad->geo = box;

    kb->data = calloc(1, sizeof(int));
    *(int *)(kb->data) = scratchpad_id;
    scratchpad_id++;
    return true;

err:
    wlr_log(WLR_ERROR, "Config line %i malformed/incomplete.", line);
    return false;
}


static struct {
    const char *name;
    const action action;
    bool (*data_handler)(struct binding *kb, char *data, int line);
} action_map[] = {
    { "shutdown", &shutdown, NULL },
    { "exec", &exec_command, &str_handler },
    { "close_window", &close_window, NULL },
    { "move_window", &move_window, NULL },
    { "focus", &focus_in_direction, &dir_handler },
    { "next_desk", &next_desk, NULL },
    { "prev_desk", &prev_desk, NULL },
    { "pan_desk", &pan_desk, &motion_handler },
    { "reset_zoom", &reset_zoom, NULL },
    { "zoom", &zoom, &str_handler },
    { "set_mark", &set_mark, NULL },
    { "go_to_mark", &go_to_mark, NULL },
    { "toggle_fullscreen", &toggle_fullscreen, NULL },
    { "halfimize", &halfimize, &dir_handler },
    { "maximize", &maximize, NULL },
    { "reload_config", &reload_config, NULL },
    { "send_to_desk", &send_to_desk, &str_handler },
    { "scratchpad", &toggle_scratchpad, &scratchpad_handler },
};


static struct {
    const char *name;
    const action action;
    const action begin;
} pinch_map[] = {
    { "zoom", &zoom_pinch, &zoom_pinch_begin },
};


static struct {
    const char *name;
    const action action;
} scroll_map[] = {
    { "zoom", &zoom_scroll },
    { "pan_desk", &pan_desk },
};


static bool assign_action(const char *name, char *data, struct binding *kb, int line) {
    kb->action = NULL;
    kb->data = NULL;
    size_t i;

    switch (kb->key) {
	case PINCH:
	    for (i = 0; i < sizeof(pinch_map) / sizeof(pinch_map[0]); i++) {
		if (strcmp(pinch_map[i].name, name) == 0) {
		    kb->action = pinch_map[i].action;
		    kb->begin = pinch_map[i].begin;
		    return true;
		}
	    }
	    break;
	case SCROLL:
	    for (i = 0; i < sizeof(scroll_map) / sizeof(scroll_map[0]); i++) {
		if (strcmp(scroll_map[i].name, name) == 0) {
		    kb->action = scroll_map[i].action;
		    return true;
		}
	    }
	    break;
	default:
	    for (i = 0; i < sizeof(action_map) / sizeof(action_map[0]); i++) {
		if (strcmp(action_map[i].name, name) == 0) {
		    kb->action = action_map[i].action;
		    if (action_map[i].data_handler != NULL) {
			if (!action_map[i].data_handler(kb, data, line)){
			    return false;
			}
		    }
		    return true;
		}
	    }
    }

    return false;
}


static void free_binding(struct binding *kb) {
    if (kb->data)
	free(kb->data);
    wl_list_remove(&kb->link);
    free(kb);
}


static void add_binding(char *data, const int line) {
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
    if (kb->key == XKB_KEY_NoSymbol) {
	kb->key = xkb_keysym_from_name(s, XKB_KEYSYM_CASE_INSENSITIVE);
    }

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
    if (!s) {
	wlr_log(WLR_ERROR, "Config line %i: malformed option.", line);
	return;
    }
    if (!assign_action(s, strtok(NULL, "\n\r"), kb, line)) {
	free(kb);
	return;
    }

    struct binding *kb_existing, *tmp;
    struct wl_list *bindings;
    if (is_mouse_binding) {
	bindings = &wimp.mouse_bindings;
    } else {
	bindings = &wimp.key_bindings;
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


static void setup_vt_switching() {
    if (!wimp.vt_switching) {
	return;
    }

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


struct source {
    struct wl_event_source *wl_event_source;
};


static int _auto_start(void *data) {
    struct source *source = data;
    wl_event_source_remove(source->wl_event_source);
    free(source);

    char *script = wimp.auto_start;
    if (fork() == 0) {
	if (execl(script, script, (void *)NULL) == -1) {
	    exit(EXIT_FAILURE);
	}
    } else {
	wlr_log(WLR_DEBUG, "Running autostart script: %s", script);
    }
    free(wimp.auto_start);
    return 0;
}


void schedule_auto_start() {
    if (wimp.auto_start) {
	wimp.auto_start = expand(wimp.auto_start);
    } else {
	wimp.auto_start = strndup(
	    wimp.config_directory, strlen(wimp.config_directory) + strlen("autostart")
	);
	strcat(wimp.auto_start, "autostart");
    }

    if (is_executable(wimp.auto_start)) {
	struct wl_event_loop *event_loop = wl_display_get_event_loop(wimp.display);
	struct source *source = calloc(1, sizeof(struct source));
	source->wl_event_source = wl_event_loop_add_timer(event_loop, _auto_start, source);
	wl_event_source_timer_update(source->wl_event_source, 1);
    }
}


static void parse_config(FILE *stream) {
    char *s;
    char linebuf[1024];
    int line = 0;
    int index;

    while (fgets(linebuf, sizeof(linebuf), stream)) {
	line++;
	if (!(s = strtok(linebuf, " \t\n\r")) || s[0] == '#') {
	    continue;
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
		index = strtod(s, NULL) - 1;
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
		    continue;
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

		// desk <index> corners resize <#rrggbb>
		else if (!strcasecmp(s, "corners")) {
		    s = strtok(NULL, " \t\n\r");
		    if (!strcasecmp(s, "resize")) {
			assign_colour(strtok(NULL, " \t\n\r"), desk->corner_resize);
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

	// vt_switching [off|on]
	else if (!strcasecmp(s, "vt_switching")) {
	    s = strtok(NULL, " \t\n\r");
	    if (!strcasecmp(s, "off"))
		wimp.vt_switching = false;
	    else if (!strcasecmp(s, "on"))
		wimp.vt_switching = true;
	}

	// scroll_direciton [natural|reverse]
	else if (!strcasecmp(s, "scroll_direction")) {
	    s = strtok(NULL, " \t\n\r");
	    if (!strcasecmp(s, "natural"))
		wimp.reverse_scrolling = false;
	    else if (!strcasecmp(s, "reverse"))
		wimp.reverse_scrolling = true;
	}

	// e.g.: set_modifier logo
	else if (!strcasecmp(s, "set_modifier")) {
	    if ((s = strtok(NULL, " \t\n\r"))) {
		enum wlr_keyboard_modifier mod = get(mods, s);
		if (mod)
		    wimp.mod = mod;
		else
		    wlr_log(WLR_ERROR, "Config line %i: invalid modifier name '%s'.", line, s);
	    }
	}

	// bind [<modifiers>] <key> <action>
	else if (!strcasecmp(s, "bind")) {
	    add_binding(strtok(NULL, ""), line);
	}

	// auto_start ~/script/to/execute.sh
	else if (!strcasecmp(s, "auto_start")) {
	    wimp.auto_start = strdup(strtok(NULL, " \t\n\r"));
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

	// other non-empty, non-commented lines
	else {
	    wlr_log(WLR_ERROR, "Config line %i: unknown option '%s'.", line, s);
	}
    }
}


void load_config() {
    FILE *stream;

    // remove existing keybindings
    struct binding *kb, *tmp;
    wl_list_for_each_safe(kb, tmp, &wimp.mouse_bindings, link) {
	free_binding(kb);
    }
    wl_list_for_each_safe(kb, tmp, &wimp.key_bindings, link) {
	free_binding(kb);
    }

    drop_scratchpads();

    // load defaults
    char *buffer = NULL;
    size_t size = 0;
    stream = open_memstream(&buffer, &size);
    fprintf(stream, DEFAULT_CONFIG);
    rewind(stream);
    parse_config(stream);
    free(buffer);
    fclose(stream);

    // load configuration file
    if (is_readable(wimp.config_file)) {
	stream = fopen(wimp.config_file, "r");
	parse_config(stream);
	fclose(stream);
    } else {
	wlr_log(WLR_ERROR, "%s not accessible. Using defaults.", wimp.config_file);
    }

    setup_vt_switching();
}


void locate_config() {
    char *xdg_config = getenv("XDG_CONFIG_HOME");
    char *config_dir = (xdg_config && *xdg_config) ? CONFIG_HOME_XDG : CONFIG_HOME;
    wimp.config_directory = strdup(config_dir);
    wimp.config_directory = expand(wimp.config_directory);
    config_dir = wimp.config_directory;

    if (wimp.config_file == NULL) {
	wimp.config_file = strndup(config_dir, strlen(config_dir) + strlen("config"));
	strcat(wimp.config_file, "config");
    } else {
	wimp.config_file = expand(wimp.config_file);
    };
}
