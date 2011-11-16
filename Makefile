CFLAGS_DBG?=	-ggdb
CFLAGS_OPT?=	-O2
CFLAGS_WARN?=	-Wall -W -Wextra -Wshadow -Wformat-security \
		-std=c99 -pedantic -Wbad-function-cast \
		-Wcast-align -Wcast-qual -Wchar-subscripts -Winline \
		-Wnested-externs -Wpointer-arith \
		-Wredundant-decls -Wstrict-prototypes

CFLAGS?=	${CFLAGS_DBG} ${CFLAGS_OPT}
CFLAGS+=	${CFLAGS_WARN}

CFLAGS+=	`pkg-config --cflags fuse`
LIBS+=		`pkg-config --libs fuse`

PREFIX ?= /usr/local

INSTALL_PRG = fjfs
INSTALL_PRG_DIR = $(subst //,/,$(DESTDIR)/$(PREFIX)/bin)

.PHONY: distclean clean install uninstall

all: fjfs

fjfs: fjfs.c
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o $@ fjfs.c ${LIBS}

clean:
	rm -f fjfs

distclean: clean

install: all
	install -d "$(INSTALL_PRG_DIR)"
	install $(INSTALL_PRG) "$(INSTALL_PRG_DIR)"

uninstall:
	rm $(INSTALL_PRG_DIR)/$(INSTALL_PRG)
