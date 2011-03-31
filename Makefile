CFLAGS_DBG?=	-ggdb
CFLAGS_OPT?=	-O2
CFLAGS_WARN?=	-Wall -Wextra -Wshadow -Wformat-security

CFLAGS?=	${CFLAGS_DBG} ${CFLAGS_OPT}
CFLAGS+=	${CFLAGS_WARN}

all: filejoinfs

filejoinfs: filejoinfs.c
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} `pkg-config fuse --cflags --libs` filejoinfs.c -lfuse -o $@

clean:
	rm filejoinfs
