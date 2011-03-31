CFLAGS_DBG?=	-ggdb
CFLAGS_OPT?=	-O2
CFLAGS_WARN?=	-Wall -Wextra -Wshadow -Wformat-security

CFLAGS?=	${CFLAGS_DBG} ${CFLAGS_OPT}
CFLAGS+=	${CFLAGS_WARN}

CFLAGS+=	`pkg-config --cflags fuse`
LIBS+=		`pkg-config --libs fuse`

all: filejoinfs

filejoinfs: filejoinfs.c
	${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o $@ filejoinfs.c ${LIBS}

clean:
	rm filejoinfs
