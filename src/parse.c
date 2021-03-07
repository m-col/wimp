#include <wlr/types/wlr_box.h>

#include "parse.h"
#include "types.h"


/* To get the length of these arrays we need to calculate that before passing
 * them to the getter so the macro in the header hides this. */
int _get(struct value_map *values, const int len, const char *name) {
    for (int i = 0; i < len; i++) {
	if (strcasecmp(values[i].name, name) == 0)
	    return values[i].value;
    }
    return 0;
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


static struct value_map dirs[] = {
    { "up", UP },
    { "right", RIGHT },
    { "down", DOWN },
    { "left", LEFT },
};


bool dir_handler(void **data, char *args) {
    *data = calloc(1, sizeof(enum direction));
    *(enum direction *)(*data) = get(dirs, strtok(args, " \t\n\r"));
    return true;
}


bool str_handler(void **data, char *args) {
    if (!args) {
	return false;
    }
    if (is_number(args)) {
	*data = calloc(1, sizeof(double));
	*(double *)(*data) = strtod(args, NULL);
    } else {
	*data = calloc(strlen(args), sizeof(char));
	strncpy(*data, args, strlen(args));
    }
    return true;
}


bool motion_handler(void **data, char *args) {
    char *s;
    if (!args) {
	return false;
    }

    s = strtok(args, " \t\n\r");
    if (!is_number(s) || !args) {
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
    *data = calloc(1, sizeof(struct motion));
    *(struct motion *)(*data) = motion;
    return true;
}


static int _scratchpad_id = 0;


bool scratchpad_handler(void **data, char *args) {
    char *geo, *command;
    struct wlr_box box;
    if (!args) {
	return false;
    }
    if (!(geo = strtok(args, " \t\n\r"))) {
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

    *data = calloc(1, sizeof(int));
    *(int *)(*data) = _scratchpad_id;
    _scratchpad_id++;
    return true;
}


bool box_handler(void **data, char *args) {
    char *geo;
    if (!(geo = strtok(args, " \t\n\r"))) {
	return false;
    }
    *data = calloc(1, sizeof(struct wlr_box));
    if (!wlr_box_from_str(geo, *data)) {
	free(*data);
	return false;
    }
    return true;
}
