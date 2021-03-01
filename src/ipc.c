#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "action.h"
#include "config.h"
#include "ipc.h"
#include "keybind.h"

#define SOCKET_PATH "/tmp/wimpy-sock-%s"


void close_ipc(const char *display) {
    char path[1024];
    snprintf(path, sizeof(path), SOCKET_PATH, display);
    unlink(path);
}


static void handle_message(char *message, char *response) {
    char *s = strtok(message, " \t\n\r");

    // set <option> <value>
    if (!strcasecmp(s, "set")) {
	set_configurable(s, response);
    }

    // bind [<modifiers>] <key> <action>
    else if (!strcasecmp(s, "bind")) {
	add_binding(s, response);
    }

    // <action> <data>
    else {
	if (!do_action(message, response)) {
	    sprintf(response, "Sorry, I don't recognise that command.");
	}
    }
}


static int dispatch(int sock, unsigned int mask, void *data) {
    char message[1024];
    char response[1024] = {0};

    if (mask & WL_EVENT_READABLE) {
	int fd = accept(sock, NULL, NULL);
	if (fd == -1) {
	    wlr_log(WLR_ERROR, "Failed to accept connection from client.");
	    return 0;
	}
	ssize_t len = recv(fd, message, sizeof(message) - 1, 0);
	message[len] = '\0';
	handle_message(message, response);
	if (response[0]) {
	    send(fd, response, strlen(response), 0);
	}
	close(fd);
    }

    return 0;
}


void set_up_defaults(){
    char defaults[][64] = {
	"set desks 2",
	"set desk 2 background #3e3e73",
	"set desk 2 borders normal #31475c",
	"set desk 2 corners normal #3e5973",
	"set mark_indicator #47315c",
	"set snap_box 47315c66",
	"set mod Logo",
	"set vt_switching on",
	"set bind_marks on",
	"bind Ctrl Escape terminate",
    };
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++) {
	handle_message(defaults[i], NULL);
    }
}


bool start_ipc(const char *display) {
    int sock;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	wlr_log(WLR_ERROR, "Failed to create IPC socket.");
	return false;
    }

    struct sockaddr_un addr;
    snprintf(addr.sun_path, sizeof(addr.sun_path), SOCKET_PATH, display);
    addr.sun_family = AF_UNIX;
    unlink(addr.sun_path);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
	wlr_log(WLR_ERROR, "Failed to bind IPC socket.");
	return false;
    }

    if (listen(sock, SOMAXCONN) == -1) {
	wlr_log(WLR_ERROR, "Failed to listen on IPC socket.");
	return false;
    }

    struct wl_event_loop *event_loop = wl_display_get_event_loop(wimp.display);
    wl_event_loop_add_fd(event_loop, sock, WL_EVENT_READABLE, &dispatch, NULL);

    return true;
}
