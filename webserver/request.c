/*
 * request.c: Does the bulk of the work for the web server.
 */

#include "common.h"
#include "request.h"

struct request {
	int fd;		 /* descriptor for client connection */
	struct file_data *data;
};

/* requestError(fd, filename, "404", "Not found", 
 *		"OS server could not find this file");
 */
static void
request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXBUF];
	int i;
	unsigned int csum = 0;

	/* create the body of the error message */
	sprintf(body, "<html><title>OS Web Server Error</title>");
	sprintf(body + strlen(body), "<body bgcolor=" "fffff" ">\r\n");
	sprintf(body + strlen(body), "<p>%s: %s</p>\r\n", errnum, shortmsg);
	sprintf(body + strlen(body), "<p>%s: %s</p>\r\n", longmsg, cause);
	sprintf(body + strlen(body), "</body></html>\r\n");

	/* write out the header information for this response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_write(fd, buf, strlen(buf));
	printf("%s", buf);

	sprintf(buf, "Content-Type: text/html\r\n");
	Rio_write(fd, buf, strlen(buf));
	printf("%s", buf);

	sprintf(buf, "Content-Length: %ld\r\n", strlen(body));
	Rio_write(fd, buf, strlen(buf));
	printf("%s", buf);

	/* generate a very trivial checksum */
	for (i = 0; i < strlen(body); i++) {
		csum += (unsigned char)(body[i]);
	}
	sprintf(buf, "Content-Csum: %u\r\n\r\n", csum);
	Rio_write(fd, buf, strlen(buf));
	printf("%s", buf);

	/* write out the content */
	Rio_write(fd, body, strlen(body));
	printf("%s", body);

}

/* reads and discards everything up to an empty text line */
static void
request_read_headers(struct rio *rp)
{
	char buf[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	while (strcmp(buf, "\r\n")) {
		Rio_readlineb(rp, buf, MAXLINE);
	}
	return;
}


/* Calculates filename from uri. 
 * for this simple server, filename = .uri
 *
 * Adding the "./" means that files will only be served from the directory in
 * which the webserver is running.
 *
 * Also, we don't serve files with a .. in the path (see request_readfile). */
static void
request_parse_URI(char *uri, char *filename, size_t max)
{
	snprintf(filename, max, "./%s", uri);
}

/* Fills in the filetype given the filename */
static void
request_get_file_type(char *filename, char *filetype)
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else
		strcpy(filetype, "text/plain");
}

/* entry point to this file */
/* returns a pointer to a request struct, filling rq->fd with connfd,
 * and rq->file_name with the file that is being requested.
 * Returns NULL on failure.
 */
struct request *
request_init(int connfd, struct file_data *data)
{
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	struct rio *rio;
	struct request *rq;

	assert(data);
	rq = Malloc(sizeof(struct request));
	rq->fd = connfd;
	rq->data = data;
	data->file_name = Malloc(MAXLINE);
	data->file_buf = NULL;
	data->file_size = 0;
	rio = Rio_init(rq->fd);
	Rio_readlineb(rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);

	// printf("%s %s %s, fd = %d\n", method, uri, version, connfd);
	if (strcasecmp(method, "GET")) {
		request_error(rq->fd, method, "501", "Not Implemented",
			     "OS Web Server does not implement this method");
		Rio_destroy(rio);
		request_destroy(rq);
		return NULL;
	}
	request_read_headers(rio);
	request_parse_URI(uri, data->file_name, MAXLINE);
	Rio_destroy(rio);
	return rq;
}

void
request_destroy(struct request *rq)
{
	assert(rq);
	/* close the connection fd */
	SYS(close(rq->fd));
	free(rq);
}

/* read in filename corresponding to request. 
 * Returns 1 on success, and fills rq->file_buf, and rq->file_size.
 * Returns 0 on failure, sends error to client. */
int
request_readfile(struct request *rq)
{
	int srcfd;
	struct stat sbuf;
	struct file_data *data;
	char *ext;

	data = rq->data;
	assert(data);

	/* don't serve files that start with /, or .., or end in .c */
	if (data->file_name[0] == '/') {
		/* this shouldn't really happen because we add a "./" at the
		 * beginning of the file path */
		request_error(rq->fd, data->file_name, "404", "Not found",
			      "OS Web Server doesn't serve files "
			      "with absolute paths");
		return 0;
	}
	if (strstr(data->file_name, "..") != NULL) {
		request_error(rq->fd, data->file_name, "404", "Not found",
			      "OS Web Server doesn't serve files "
			      "with .. in the path");
		return 0;
	}
	if (((ext = strrchr(data->file_name, '.')) != NULL) && 
	    ((strcmp(ext, ".c") == 0) || (strcmp(ext, ".h") == 0))) {
		request_error(rq->fd, data->file_name, "404", "Not found",
			      "OS Web Server doesn't serve C or header files ");
		return 0;
	}

	if (stat(data->file_name, &sbuf) < 0) {
		request_error(rq->fd, data->file_name, "404", "Not found",
			      "OS Web Server could not find this file");
		return 0;
	}
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
		request_error(rq->fd, data->file_name, "403", "Forbidden",
			      "OS Web Server could not read this file");
		return 0;
	}

	data->file_size = sbuf.st_size;

	if (data->file_size) {
		SYS(srcfd = open(data->file_name, O_RDONLY, 0));
		data->file_buf = Malloc(data->file_size);
		Rio_read(srcfd, data->file_buf, data->file_size);
		/* ask the kernel to stop caching the file */
		SYS(posix_fadvise(srcfd, 0, data->file_size, 
				  POSIX_FADV_DONTNEED));
		SYS(close(srcfd));
		/* we add this delay to simulate a disk. otherwise, file caching
		 * doesn't have much benefit because a lot of the time is spent
		 * in processing (see request_processfile below) and so
		 * request_readfile does not have much impact. */
		/* we don't need to add this delay any longer. */
		/* usleep(1000); */
	}
	return 1;
}

/* if you have previous file data, you can reuse it */
void
request_set_data(struct request *rq, struct file_data *data)
{
	rq->data = data;
}

/* add some file processing delay.
 *
 * previously, the main reason for this function was that if we didn't do enough
 * processing on the file, the network became the bottleneck, and then the
 * various server parameters had no affect on server performance. This is not a
 * problem any longer. */
static void
request_processfile(struct request *rq)
{
	struct file_data *data;
	int i, j;
	volatile int dummy = 0;
	data = rq->data;
	assert(data);

	for (i = 0; i < 8; i++) {
		for (j = 0; j < data->file_size; j++) {
			dummy += (unsigned char)(data->file_buf[j]);
		}
	}
}

/* send filename to the fd connection */
void
request_sendfile(struct request *rq)
{
	char filetype[MAXLINE], buf[MAXBUF];
	int i;
	unsigned int csum = 0;
	struct file_data *data;
	long size = 0;

	data = rq->data;
	assert(data);

	request_get_file_type(data->file_name, filetype);
	/* generate a very trivial checksum */
	for (i = 0; i < data->file_size; i++) {
		csum += (unsigned char)(data->file_buf[i]);
	}
	/* do some processing */
	request_processfile(rq);
	/* put together response */
	size += sprintf(buf + size, "HTTP/1.0 200 OK\r\n");
	size += sprintf(buf + size, "Server: OS Web Server\r\n");
	size += sprintf(buf + size, "Content-Type: %s\r\n", filetype);
	size += sprintf(buf + size, "Content-Length: %d\r\n", data->file_size);
	size += sprintf(buf + size, "Content-Csum: %u\r\n\r\n", csum);

	Rio_write(rq->fd, buf, strlen(buf));

	/* writes data->file_buf to the client socket */
	if (data->file_size > 0) {
		Rio_write(rq->fd, data->file_buf, data->file_size);
	}
}
