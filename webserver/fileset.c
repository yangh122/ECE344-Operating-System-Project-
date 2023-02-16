#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <popt.h>
#include "common.h"

/* Generate a set of files for the webserver assignment */

poptContext context;	/* context for parsing command-line options */

static void
usage()
{
	poptPrintUsage(context, stderr, 0);
	exit(1);
}

/* mean file size is in terms of 4K, so it is 12k */
#define DEFAULT_MEAN_FILE_SZ 3
/* this will create roughly 12k * 256 = 3MB of files */
#define DEFAULT_NR_FILES 256
/* the directory in which to create the files */
#define DEFAULT_DIR fileset_dir

static int default_file_sz = DEFAULT_MEAN_FILE_SZ;
static int default_nr_files = DEFAULT_NR_FILES;
static char *dir = STR(DEFAULT_DIR);

int
main(int argc, const char *argv[])
{
	char c;
	int nr_files = 0;
	DIR *d;
	char filename[1024];
	int current_fileset_sz = 0;
	int total_fileset_sz;
	int fd_idx;
	char idx_buf[4096];
	char idx_buffer[4096 * 256]; // a large buffer
	int cur_size = 0;

	struct poptOption options_table[] = {
		{NULL, 'm', POPT_ARG_INT, &default_file_sz, 'm',
		 "mean file size",
		 " default: " STR(DEFAULT_MEAN_FILE_SZ)},
		{NULL, 'n', POPT_ARG_INT, &default_nr_files, 'n',
		 "number of files",
		 " default: " STR(DEFAULT_NR_FILES)},
		{NULL, 'd', POPT_ARG_STRING, &dir, 'd',
		 "directory in which the files are created",
		 " default: " STR(DEFAULT_DIR)},
		POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
	};

	context = poptGetContext(NULL, argc, argv, options_table, 0);
	while ((c = poptGetNextOpt(context)) >= 0);
	if (c < -1) {	/* an error occurred during option processing */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(context, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));
		exit(1);
	}
	if (default_file_sz <= 1) {
		fprintf(stderr, "mean file size is too small\n");
		usage();
	}
	if (default_nr_files < 1 || default_nr_files > 99999) {
		fprintf(stderr, "nr of files is out of bounds\n");
		usage();
	}
	if (strlen(dir) > 1000) {
		fprintf(stderr, "dir name is too long\n");
		usage();
	}
	d = opendir(dir);
	if (d) { /* directory exists */
		struct dirent *p;
		while ((p = readdir(d)) != NULL) {
			char buf[4096];
			struct stat statbuf;
			sprintf(buf, "%s/%s", dir, p->d_name);
			if (stat(buf, &statbuf) >= 0) {
				if (S_ISREG(statbuf.st_mode)) {
					unlink(buf);
				}
			}
		}
		closedir(d);
	} else {
		if (mkdir(dir, 0755) < 0) {
			fprintf(stderr, "mkdir: %s: %s\n", dir, 
				strerror(errno));
			exit(1);
		}
	}
	// fix the seed of the random number generator, or else the file size
	// distributions vary too much, leading to high variance in results.
	// note that the client still uses a random seed to request files.
	// init_random();
	srandom(100);
	total_fileset_sz = default_file_sz * 4096 * default_nr_files;
	/* null terminate the buffer */
	idx_buffer[0] = 0;
	while (current_fileset_sz < total_fileset_sz) {
		char name[10];
		int fd, file_sz, remaining;
		char buf[4096];
		double ms = default_file_sz;
		unsigned int csum = 0;
		int j;

		strcpy(filename, dir);
		sprintf(name, "/%05d", nr_files++);
		strcat(filename, name);
		file_sz = rand_pareto(4096, ms/(ms - 1));
		current_fileset_sz += file_sz;
		SYS(fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644));
		remaining = file_sz;
		while (remaining > 0) {
			int sz = (remaining < 4096) ? remaining : 4096;
			for (j = 0; j < sz; j++) {
				/* printable characters lie between 0x20-0x73 */
				buf[j] = random() % (0x73 - 0x20) + 0x20;
				csum += (unsigned char)(buf[j]);
			}
			Rio_write(fd, buf, sz);
			remaining -= sz;
		}
		SYS(close(fd));
		printf("filename = %s, csum = %u, len = %d\n", filename, csum,
		       file_sz);
		sprintf(idx_buf, "%s %u %d\n", filename, csum, file_sz);
		cur_size += strlen(idx_buf);
		/* avoid buffer overflow */
		assert(cur_size < (4096 * 256 - 1));
		strcat(idx_buffer, idx_buf);
	}

	strcpy(filename, dir);
	strcat(filename, ".idx");
	SYS(fd_idx = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644));

	/* write the number of files in the index file */
	sprintf(idx_buf, "%d\n", nr_files);
	Rio_write(fd_idx, idx_buf, strlen(idx_buf));

	/* write the rest of the buffer */
	Rio_write(fd_idx, idx_buffer, strlen(idx_buffer));
	SYS(close(fd_idx));
	
	printf("file set size = %d, nr files = %d\n"
	       "mean file size = %d, expected mean file size = %d\n",
	       current_fileset_sz, nr_files,
	       (int)((double)current_fileset_sz / nr_files),
	       default_file_sz * 4096);
	exit(0);
}
