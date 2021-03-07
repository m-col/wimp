#include "action.h"
#include "keybind.h"
#include "parse.h"
#include "types.h"


static struct dict mods[] = {
    { "shift", WLR_MODIFIER_SHIFT },
    { "caps", WLR_MODIFIER_CAPS },
    { "ctrl", WLR_MODIFIER_CTRL },
    { "alt", WLR_MODIFIER_ALT },
    { "mod2", WLR_MODIFIER_MOD2 },
    { "mod3", WLR_MODIFIER_MOD3 },
    { "logo", WLR_MODIFIER_LOGO },
    { "mod5", WLR_MODIFIER_MOD5 },
};


static struct dict mouse_keys[] = {
    { "motion", MOTION },
    { "scroll", SCROLL },
    { "drag1", DRAG1 },
    { "drag2", DRAG2 },
    { "drag3", DRAG3 },
    { "pinch", PINCH },
};


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
    if (!get_action(s, &kb->action, strtok(NULL, "\n\r"), &kb->data, response, kb->key)) {
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
