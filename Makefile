CFLAGS = -ggdb -Wall -Wextra -Wshadow -Wformat-security -O2
all: filejoinfs
filejoinfs: filejoinfs.c
	${CC} ${CFLAGS} `pkg-config fuse --cflags --libs` filejoinfs.c -lfuse -o $@
clean:
	rm filejoinfs
