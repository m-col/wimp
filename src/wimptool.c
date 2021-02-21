#include <stdio.h> 
#include <stdlib.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/wimpy-sock-%s"


void usage() {
    fprintf(stdout, "Usage: wimptool <command> <arguments>\n");
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
	usage();
	return EXIT_FAILURE;
    }

    char *display = getenv("WAYLAND_DISPLAY");
    if (!display) {
	fprintf(stderr, "WAYLAND_DISPLAY not set.\n");
	return EXIT_FAILURE;
    }

    int sock;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	fprintf(stderr, "Failed to create socket.\n");
	return EXIT_FAILURE;
    }

    struct sockaddr_un addr;
    snprintf(addr.sun_path, sizeof(addr.sun_path), SOCKET_PATH, display);
    addr.sun_family = AF_UNIX;

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
	fprintf(stderr, "Failed to connect to wimp.\n");
	return EXIT_FAILURE;
    }

    char buffer[BUFSIZ] = {0};
    for (int i = 1; i < argc; i++) {
	strcat(buffer, argv[i]);
	strcat(buffer, " ");
    }

    if (send(sock, buffer, strlen(buffer), 0) == -1) {
	fprintf(stderr, "Failed to send message to wimp.\n");
	return EXIT_FAILURE;
    }

    char response[BUFSIZ] = {0};
    ssize_t len = recv(sock, response, sizeof(response) - 1, 0);
    if (len < 0) {
	fprintf(stderr, "Error receiving a response.\n");
	return EXIT_FAILURE;
    }

    if (len > 0) {
	response[len] = '\0';
	fprintf(stdout, "wimp: %s\n", response);
    }
    close(sock);
    return EXIT_SUCCESS;
}
