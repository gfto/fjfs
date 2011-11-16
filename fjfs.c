/*
 * fjfs
 * fjfs is FUSE module that implements virtual joining of multiple files as one.
 *
 * Copyright (c) 2010-2011 Georgi Chorbadzhiyski (georgi@unixsol.org)
 * All rights reserved.
 *
 * Redistribution and use of this script, with or without modification, is
 * permitted provided that the following conditions are met:
 *
 * 1. Redistributions of this script must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 *  EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500 // for "pread"
#define _GNU_SOURCE 1 // for "getline"

#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fuse.h>

/* Handle list of files */
struct fileinfo {
	int		fd;
	char	*name;
	off_t	size;
};

struct files {
	int		num_files;
	off_t	total_size;
	int		alloc_files;
	struct fileinfo **data;
};

struct files *filelist;

struct files *files_alloc(void) {
	struct files *f = calloc(1, sizeof(struct files));
	f->alloc_files = 64;
	f->data = calloc(f->alloc_files, sizeof(struct fileinfo *));
	return f;
}

void files_free(struct files **pfiles) {
	struct files *files = *pfiles;
	struct fileinfo *file;
	int i;
	if (files) {
		for (i=0; i<files->num_files; i++) {
			file = files->data[i];
			close(file->fd);
			free(file->name);
			free(file);
		}
		free(files->data);
		free(*pfiles);
		*pfiles = NULL;
	}
}

#if 0
void files_dump(struct files *files) {
	int i;
	fprintf(stdout,"num_files:%d\n", files->num_files);
	fprintf(stdout,"alloc_files:%d\n", files->alloc_files);
	fprintf(stdout,"total_sizes:%lld\n", (unsigned long long)files->total_size);
	for (i=0; i<files->num_files; i++) {
		struct fileinfo *f = files->data[i];
		fprintf(stdout,"file[%d]->fd=%d\n", i, f->fd);
		fprintf(stdout,"file[%d]->name=%s\n", i, f->name);
		fprintf(stdout,"file[%d]->size=%llu\n", i, (unsigned long long)f->size);
	}
}
#endif

int files_add_file(struct files *files, char *filename) {
	int ret = 0;
	struct stat sb;
	if (stat(filename, &sb) != -1) {
		struct fileinfo *file;

		int fd = open(filename, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "open(%s) error : %s\n", filename, strerror(errno));
			return ret;
		}

		file = calloc(1, sizeof(struct fileinfo));
		file->name = strdup(filename);
		file->size = sb.st_size;
		file->fd   = fd;

		files->total_size += file->size;

		files->data[files->num_files] = file;
		files->num_files++;
		if (files->num_files >= files->alloc_files-1) {
			files->alloc_files *= 2;
			files->data = realloc(files->data, files->alloc_files * sizeof(struct fileinfo *));
		}

		ret = 1;
	} else {
		fprintf(stderr, "stat(%s) error : %s\n", filename, strerror(errno));
	}
	return ret;
}

int files_load_filelist(struct files *files, char *filename) {
	size_t len;
	ssize_t readed;
	int ret = 0;
	char *line = NULL;

	FILE *file = fopen(filename, "r");
	if (!file) {
		fprintf(stderr, "Can't open %s : %s\n", filename, strerror(errno));
		return ret;
	}

	while ((readed = getline(&line, &len, file)) != -1) {
		line[readed-1] = '\0';
		ret += files_add_file(files, line);
	}
	free(line);
	fclose(file);

	return ret;
}

static int fuse_getattr(const char *path, struct stat *stbuf) {
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	if (fstat(filelist->data[0]->fd, stbuf) == -1)
		return -errno;

	stbuf->st_size = filelist->total_size;

    return 0;
}

static int fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	(void)fi; // prevent warning
	int i;
	struct fileinfo *file = filelist->data[0];
	ssize_t readen, totreaden = 0;
	off_t fileofs = offset, passed = 0;
	int filenum = 0;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	if (offset >= filelist->total_size) // Hmm :)
		return 0;

	if (offset + size > filelist->total_size) { // Asking for too much
		if (filelist->total_size < offset) // Prevent overflow
			return 0;
		size = filelist->total_size - offset;
	}

	if (offset > file->size) { // After the first file
		// Find the file that corresponds to the required offset
		// Can be slow with lots of files, but do it simple for now
		for (i=0; i<filelist->num_files; i++) {
			struct fileinfo *f = filelist->data[i];
			passed += f->size;
			if (passed >= offset) {
				file = f;
				filenum = i;
				fileofs = file->size - (passed - offset);
				break;
			}
		}
	}

	// Read all data
	do {
		readen = pread(file->fd, buf, size, fileofs);
		if (readen == -1) // Error reading
			return -errno;
		totreaden += readen;
		fileofs += readen;
		buf += readen;
		size -= readen;
		if (fileofs >= file->size) {
			fileofs = 0;
			filenum++;
			if (filenum >= filelist->num_files) {
				break;
			}
			file = filelist->data[filenum];
		}
	} while(size > 0);

	return totreaden;
}

static struct fuse_operations concatfs_op = {
	.getattr	= fuse_getattr,
	.read		= fuse_read,
};

int main(int argc, char *argv[]) {
	int ret;
	char *filenames;
	char *mountpoint_file;
	struct stat sb;

	if (argc < 3) {
		fprintf(stderr, "Usage: fjfs filelist.txt mount-point-file\n");
		exit(EXIT_FAILURE);
	}

	filenames = argv[1];
	mountpoint_file = argv[2];

	if (stat(mountpoint_file, &sb) == -1) {
		fprintf(stderr, "Can't mount on %s : %s\n", mountpoint_file, strerror(errno));
		exit(EXIT_FAILURE);
	} else {
		if (!S_ISREG(sb.st_mode)) {
			fprintf(stderr, "%s is not a file!\n", mountpoint_file);
			exit(EXIT_FAILURE);
		}
	}

	filelist = files_alloc();
	if (!files_load_filelist(filelist, filenames)) {
		fprintf(stderr, "Error no files loaded from %s.\n", argv[1]);
		files_free(&filelist);
		exit(EXIT_FAILURE);
	}

	char *fuse_argv[5];
	fuse_argv[0] = argv[0];
	fuse_argv[1] = mountpoint_file;
	fuse_argv[2] = "-o";
	fuse_argv[3] = "nonempty,allow_other,fsname=fjfs";
	fuse_argv[4]  = 0;

	ret = fuse_main(4, fuse_argv, &concatfs_op, NULL);

	files_free(&filelist);

	return ret;
}
