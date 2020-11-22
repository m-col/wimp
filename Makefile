OUT	= deskwm
SOURCES	= $(wildcard src/*.c)
OBJECTS	= $(SOURCES:.c=.o)
CFLAGS  += -g -I. -DWLR_USE_UNSTABLE -Wall -Wextra -pedantic -Wno-unused-parameter
LDFLAGS	+= $(shell pkg-config --cflags --libs wlroots) \
	    $(shell pkg-config --cflags --libs wayland-server) \
	    $(shell pkg-config --cflags --libs xkbcommon) \
	    $(shell pkg-config --cflags --libs cairo)

${OUT}: xdg-shell-protocol.h xdg-shell-protocol.c ${OBJECTS}
	@$(CC) -o ${OUT} $(OBJECTS) $(LDFLAGS)

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c -o $@ $< ${LDFLAGS}

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

xdg-shell-protocol.h:
	@$(WAYLAND_SCANNER) server-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c: xdg-shell-protocol.h
	@$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

clean:
	rm -f ${OUT} xdg-shell-protocol.h xdg-shell-protocol.c ${OBJECTS}

.DEFAULT_GOAL=${OUT}
.PHONY: clean
