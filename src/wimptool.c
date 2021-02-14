#include <stdio.h> 
#include <stdlib.h> 
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/wimpy-sock-%s"


void usage() {
    printf("Usage: wimptool <command> <arguments>\n");
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
	usage();
	return EXIT_FAILURE;
    }

    char *display = getenv("WAYLAND_DISPLAY");
    if (!display) {
	printf("WAYLAND_DISPLAY not set.\n");
	return EXIT_FAILURE;
    }

    int sock;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	printf("Failed to create socket.\n");
	return EXIT_FAILURE;
    }

    struct sockaddr_un addr;
    snprintf(addr.sun_path, sizeof(addr.sun_path), SOCKET_PATH, display);
    addr.sun_family = AF_UNIX;

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
	printf("Failed to connect to wimp.\n");
	return EXIT_FAILURE;
    }

    char buffer[BUFSIZ] = {0};
    for (int i = 1; i < argc; i++) {
	strcat(buffer, argv[i]);
    }

    if (send(sock, buffer, strlen(buffer), 0) == -1) {
	printf("Failed to send message to wimp.\n");
	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
