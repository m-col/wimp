OUT	= wimp
SOURCES	= $(wildcard src/*.c)
OBJECTS	= $(SOURCES:.c=.o)
CFLAGS  += -g -I. -DWLR_USE_UNSTABLE -Wall -Wextra -pedantic -Wno-unused-parameter
LDFLAGS	+= $(shell pkg-config --cflags --libs wlroots) \
	    $(shell pkg-config --cflags --libs wayland-server) \
	    $(shell pkg-config --cflags --libs xkbcommon) \
	    $(shell pkg-config --cflags --libs libinput) \
	    $(shell pkg-config --cflags --libs cairo) \
	    $(shell pkg-config --cflags --libs pixman-1) \
	    -lm

PREFIX    ?= /usr/local
BINPREFIX ?= $(PREFIX)/bin

${OUT}: xdg-shell-protocol wlr-layer-shell-unstable-v1-protocol ${OBJECTS}
	@$(CC) -o ${OUT} $(OBJECTS) $(LDFLAGS)

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c -o $@ $< ${LDFLAGS}

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

xdg-shell-protocol:
	@$(WAYLAND_SCANNER) server-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@.h
	@$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@.c

wlr-layer-shell-unstable-v1-protocol:
	@$(WAYLAND_SCANNER) server-header protocols/wlr-layer-shell-unstable-v1.xml $@.h
	@$(WAYLAND_SCANNER) private-code protocols/wlr-layer-shell-unstable-v1.xml $@.c

clean:
	rm -f ${OUT} *-protocol.h *-protocol.c ${OBJECTS}

install:
	mkdir -p "$(DESTDIR)$(BINPREFIX)"
	cp -pf "$(OUT)" "$(DESTDIR)$(BINPREFIX)"

uninstall:
	rm -f "$(DESTDIR)$(BINPREFIX)/$(OUT)"

.DEFAULT_GOAL=${OUT}
.PHONY: clean
