#include <malloc.h>
#include "common.h"
#include "request.h"
#include "server_thread.h"

/* 
 * server.c: A very, very simple web server
 *
 * To run:
 *  server portnum nr_threads max_requests max_cache_size
 *
 * Repeatedly handles HTTP requests sent to this port number. Most of the work
 * is done within routines written in server_thread.c and request.c
 */

static void
usage(char *program)
{
	fprintf(stderr, "Usage: %s port nr_threads max_requests "
		"max_cache_size\n", program);
	exit(1);
}

static char *fifo = "./server_exit";

/* we will use this fifo to send a message to the server to exit */
static int
open_fifo(void)
{

	int ret;
	int fd;

	unlink(fifo);
	ret = mkfifo(fifo, 0666);
	if (ret < 0) {
		perror("mkfifo");
		exit(1);
	}
	/* Without O_NONBLOCK, open will block until the other side connects */
	SYS(fd = open(fifo, O_RDONLY | O_NONBLOCK));
#if 0
	SYS(flags = fcntl(fd, F_GETFL, 0));
	SYS(fcntl(fd, F_SETFL, flags & ~O_NONBLOCK));
#endif /* 0 */
	return fd;
}

static void
close_fifo(void)
{
	unlink(fifo);
}

int
main(int argc, char *argv[])
{
	int port, nr_threads, max_requests, max_cache_size;
	int listenfd, connfd, clientlen;
	int exitfd;
	struct sockaddr_in clientaddr;
	struct server *sv;

	if (argc != 5)
		usage(argv[0]);
	port = atoi(argv[1]);
	nr_threads = atoi(argv[2]);
	max_requests = atoi(argv[3]);
	max_cache_size = atoi(argv[4]);
	if (port < 1024) {
		fprintf(stderr, "port = %d, should be >= 1024\n", port);
		usage(argv[0]);
	}
	if (nr_threads < 0 || max_requests < 0 || max_cache_size < 0) {
		fprintf(stderr, "arguments should be > 0\n");
		usage(argv[0]);
	}

	sv = server_init(nr_threads, max_requests, max_cache_size);

	listenfd = open_listenfd(port);
	exitfd = open_fifo();

	struct pollfd fds[] = {
		{exitfd, POLLIN},
		{listenfd, POLLIN},
	};
	while (1) {
		/* wait for either a client to connect or an exit event */
		SYS(poll(fds, 2, -1));
		
		if(fds[0].revents & POLLIN) { /* exit requested */
			break;
		}

		assert(fds[1].revents & POLLIN); /* connect request arrived */
		clientlen = sizeof(clientaddr);
		/* connfd is the socket descriptor the server will use to send
		 * data to the client */
		SYS(connfd = accept(listenfd, (struct sockaddr *)&clientaddr,
				    (socklen_t *) & clientlen));

		/* serve the request */
		server_request(sv, connfd);
	}

	close_fifo();
	server_exit(sv);

	/* we don't check for memory leaks using mallinfo() because pthreads
	 * caches thread state even after a thread exits so that it can reuse
	 * this state for new threads.
	 * malloc uses multiple arenas for memory allocation for improving
	 * concurrent allocation performance. at this point, we see roughly 2224
	 * bytes allocated per arena, up to a maximum of 64 arenas.
	 * see mallopt(). */

	/* pthread_exit() will cause this (the main) thread to exit.
	 * however, this process will only exit when all the worker threads have
	 * finished executing. */
	pthread_exit(0);
}
