#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "config.h"
#include "ipc.h"

#define SOCKET_PATH "/tmp/wimpy-sock-%s"


void close_ipc(const char *display) {
    char path[1024];
    snprintf(path, sizeof(path), SOCKET_PATH, display);
    unlink(path);
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
