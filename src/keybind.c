#include "action.h"
#include "keybind.h"
#include "types.h"

struct value_map {
    const char *name;
    const int value;
};


/* To get the length of these arrays we need to calculate that before passing
 * them to the getter so the macro hides this. */
static int _get(struct value_map *values, const int len, const char *name) {
    for (int i = 0; i < len; i++) {
	if (strcasecmp(values[i].name, name) == 0)
	    return values[i].value;
    }
    return 0;
}

#define get(arr, name) _get(arr, sizeof(arr) / sizeof(arr[0]), name)


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


static bool dir_handler(struct binding *kb, char *data) {
    kb->data = calloc(1, sizeof(enum direction));
    *(enum direction *)(kb->data) = get(dirs, strtok(data, " \t\n\r"));
    return true;
}


static bool str_handler(struct binding *kb, char *data) {
    if (!data) {
	return false;
    }
    if (is_number(data)) {
	kb->data = calloc(1, sizeof(double));
	*(double *)(kb->data) = strtod(data, NULL);
    } else {
	kb->data = calloc(strlen(data), sizeof(char));
	strncpy(kb->data, data, strlen(data));
    }
    return true;
}


static bool motion_handler(struct binding *kb, char *data) {
    char *s;
    if (!data) {
	return false;
    }

    s = strtok(data, " \t\n\r");
    if (!is_number(s) || !data) {
	return false;
    }
    int x = atoi(s);

    s = strtok(NULL, " \t\n\r");
    if (!is_number(s)) {
	return false;
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
}


bool wlr_box_from_str(char* str, struct wlr_box *box) {
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


static int _scratchpad_id = 0;


static bool scratchpad_handler(struct binding *kb, char *data) {
    char *geo, *command;
    struct wlr_box box;
    if (!data) {
	return false;
    }
    if (!(geo = strtok(data, " \t\n\r"))) {
	return false;
    }
    if (!(command = strtok(NULL, "\n\r"))) {
	return false;
    }
    if (!wlr_box_from_str(geo, &box)) {
	return false;
    }

    struct scratchpad *scratchpad = calloc(1, sizeof(struct scratchpad));
    wl_list_insert(wimp.scratchpads.prev, &scratchpad->link);
    scratchpad->command = strdup(command);
    scratchpad->id = _scratchpad_id;
    scratchpad->is_mapped = false;
    scratchpad->view = NULL;
    scratchpad->geo = box;

    kb->data = calloc(1, sizeof(int));
    *(int *)(kb->data) = _scratchpad_id;
    _scratchpad_id++;
    return true;
}


static bool box_handler(struct binding *kb, char *data) {
    char *geo;
    if (!(geo = strtok(data, " \t\n\r"))) {
	return false;
    }
    struct wlr_box *box = calloc(1, sizeof(struct wlr_box));
    if (!wlr_box_from_str(geo, box)) {
	return false;
    }
    kb->data = box;
    return true;
}


static struct {
    const char *name;
    const action action;
    bool (*data_handler)(struct binding *kb, char *data);
} action_map[] = {
    { "terminate", &terminate, NULL },
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
    { "send_to_desk", &send_to_desk, &str_handler },
    { "scratchpad", &toggle_scratchpad, &scratchpad_handler },
    { "to_region", &to_region, &box_handler },
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


static bool assign_action(const char *name, char *data, struct binding *kb, char * response) {
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
			if (!action_map[i].data_handler(kb, data)){
			    sprintf(response, "Command malformed/incomplete.");
			    return false;
			}
		    }
		    return true;
		}
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


void add_binding(char *message, char *response) {
    char *s;
    if (!(s = strtok(NULL, " \t\n\r"))) {
	sprintf(response, "What do you want to bind?");
	return;
    }

    struct binding *kb = calloc(1, sizeof(struct binding));
    enum wlr_keyboard_modifier mod;
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
	    sprintf(response, "No such key '%s'.", s);
	    free(kb);
	    return;
	}
	is_mouse_binding = true;
    }

    // action
    s = strtok(NULL, " \t\n\r");
    if (!s) {
	sprintf(response,  "Command malformed/incomplete.");
	return;
    }
    if (!assign_action(s, strtok(NULL, "\n\r"), kb, response)) {
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


void set_mod(char *message, char *response) {
    char *s = strtok(NULL, " \t\n\r");
    enum wlr_keyboard_modifier mod = get(mods, s);
    if (mod) {
        wimp.mod = mod;
    } else {
        sprintf(response, "Invalid modifier name '%s'.", s);
    }
}
