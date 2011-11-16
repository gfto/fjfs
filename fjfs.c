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
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include <fnmatch.h>
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

enum fl_mode {
	FL_FILE,
	FL_GLOB,
	FL_ARGS
};

struct files *filelist;
char *filenames;
char *mountpoint;
int debug = 0;
int allow_other = 0;
int mountpoint_created = 0;
enum fl_mode list_mode = FL_FILE;

static struct files *files_alloc(void) {
	struct files *f = calloc(1, sizeof(struct files));
	f->alloc_files = 64;
	f->data = calloc(f->alloc_files, sizeof(struct fileinfo *));
	return f;
}

static void files_free(struct files **pfiles) {
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

static void files_dump(struct files *files) {
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

static int files_add_file(struct files *files, char *filename) {
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

static int files_load_filelist(struct files *files, char *filename) {
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

static int files_load_glob(struct files *files, char *glob_match) {
	int i, entries, ret = 0;
	struct dirent **namelist;
	char *f1 = strdup(glob_match);
	char *f2 = strdup(glob_match);
	char *dir  = dirname(f1);
	char *match = basename(f2);

	if (debug)
		fprintf(stderr, "dir:%s match:%s req:%s\n", dir, match, glob_match);
	entries = scandir(dir, &namelist, NULL, alphasort);
	if (entries < 0) {
		fprintf(stderr, "scandir %s : %s\n", dir, strerror(errno));
		free(f1);
		free(f2);
		return 0;
	}

	char *filename = calloc(1, strlen(dir) + NAME_MAX + 16);
	for (i=0;i<entries;i++) {
		struct dirent *entry = namelist[i];
		if (fnmatch(match, entry->d_name, FNM_PATHNAME) == 0) {
			sprintf(filename, "%s/%s", dir, entry->d_name);
			ret += files_add_file(files, filename);
		}
		free(entry);
	}
	free(filename);
	free(namelist);
	free(f1);
	free(f2);
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

static int fjfs_unlink(const char *path) {
	if (strcmp(path, "/") != 0)
		return -ENOENT;
	return unlink(path);
}

static void fjfs_destroy(void *f __attribute__((unused))) {
	if (mountpoint_created)
		unlink(mountpoint);
}

static struct fuse_operations concatfs_op = {
	.getattr	= fuse_getattr,
	.read		= fuse_read,
	.unlink		= fjfs_unlink,
	.destroy	= fjfs_destroy,
};

static const char *short_options = "fgaohd";

static const struct option long_options[] = {
	{ "file", no_argument, NULL, 'f' },
	{ "glob", no_argument, NULL, 'g' },
	{ "args", no_argument, NULL, 'a' },
	{ "allow-other", no_argument, NULL, 'o' },
	{ "help", no_argument, NULL, 'h' },
	{ "debug", no_argument, NULL, 'd' },
	{ 0, 0, 0, 0 }
};

static void show_usage(void) {
	printf("fjfs - FUSE module for virtual joining of multiple files into one.\n");
	printf("\n");
	printf("Usage: fjfs [file-list-options] [options] mount-point-file file-list\n");
	printf("\n");
	printf("Note: file-list depends on the options described bellow.\n");
	printf("\n");
	printf("File list options:\n");
	printf(" -f --file | file-list is text file containing list of files (default).\n");
	printf(" -g --glob | file-list is glob (*, ?, dir/file*).\n");
	printf(" -a --args | file-list is N filenames (file1 file2 fileX).\n");
	printf("\n");
	printf("Examples:\n");
	printf(" - Join files listed in filelist.txt as test-mount.txt\n");
	printf("   fjfs test-mount.txt filelist.txt\n");
	printf("\n");
	printf(" - Join files named testfile*.txt as test-mount.txt\n");
	printf("   fjfs --glob test-mount.txt 'testfile*.txt'\n");
	printf("\n");
	printf(" - Join files named testfileX.txt testfileY.txt testfileZ.txt as test-mount.txt\n");
	printf("   fjfs --args test-mount.txt testfileX.txt testfileY.txt testfileZ.txt\n");
	printf("\n");
	printf("Other options:\n");
	printf(" -o --allow-other | Mount FUSE with allow_other option. This allows other users\n");
	printf("                  . to access the mounted fjfs instance. /etc/fuse.conf must\n");
	printf("                  . contain \"user_allow_other\" in order for this option to work.\n");
}

static void parse_parameters(int argc, char *argv[]) {
	int j;

	while ( (j = getopt_long(argc, argv, short_options, long_options, NULL)) != -1 ) {
		switch (j) {
		case 'f': list_mode = FL_FILE; break;
		case 'g': list_mode = FL_GLOB; break;
		case 'a': list_mode = FL_ARGS; break;
		case 'd': debug = 1; break;
		case 'o': allow_other = 1; break;
		case 'h':
			show_usage();
			exit(EXIT_SUCCESS);
			break;
		}
	}

	mountpoint = argv[optind];
	filenames  = argv[optind + 1];

	if (!mountpoint || !filenames) {
		show_usage();
		exit(EXIT_FAILURE);
	}

	if (debug) {
		fprintf(stderr, "mount point: %s\n", mountpoint);
		switch (list_mode) {
		case FL_FILE:
			fprintf(stderr, "list (file) : %s\n", filenames);
			break;
		case FL_GLOB:
			fprintf(stderr, "list (glob) : %s\n", filenames);
			break;
		case FL_ARGS:
			fprintf(stderr, "list (args) :");
			for (j = optind + 1; j < argc; j++) {
				fprintf(stderr, " %s", argv[j]);
			}
			fprintf(stderr, "\n");
			break;
		}
	}
}

static int init_filelist(int argc, char *argv[]) {
	int i, ret = 0;

	filelist = files_alloc();
	if (!filelist)
		return ret;

	switch (list_mode) {
	case FL_FILE:
		ret = files_load_filelist(filelist, filenames);
		break;
	case FL_GLOB:
		ret = files_load_glob(filelist, filenames);
		break;
	case FL_ARGS:
		for (i = optind + 1; i < argc; i++) {
			ret += files_add_file(filelist, argv[i]);
		}
		break;
	}

	if (debug)
		files_dump(filelist);

	if (!ret)
		fprintf(stderr, "ERROR: No files were selected for joining.\n");

	return ret;
}

static int mount_fuse(char *program_file) {
	char *fuse_argv[5];
	fuse_argv[0] = program_file;
	fuse_argv[1] = mountpoint;
	fuse_argv[2] = "-o";
	if (!allow_other)
		fuse_argv[3] = "nonempty,fsname=fjfs";
	else
		fuse_argv[3] = "nonempty,allow_other,fsname=fjfs";
	fuse_argv[4]  = 0;
	return fuse_main(4, fuse_argv, &concatfs_op, NULL);
}

int main(int argc, char *argv[]) {
	int ret = EXIT_FAILURE;
	struct stat sb;

	parse_parameters(argc, argv);

	if (stat(mountpoint, &sb) == -1) {
		FILE *f = fopen(mountpoint, "wb");
		if (f) {
			mountpoint_created = 1;
			fclose(f);
		} else {
			fprintf(stderr, "Can't create mount point %s : %s\n", mountpoint, strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		if (!S_ISREG(sb.st_mode)) {
			fprintf(stderr, "%s is not a file!\n", mountpoint);
			exit(EXIT_FAILURE);
		}
	}

	if (init_filelist(argc, argv))
		ret = mount_fuse(argv[0]);

	if (mountpoint_created)
		unlink(mountpoint);

	files_free(&filelist);

	return ret;
}
