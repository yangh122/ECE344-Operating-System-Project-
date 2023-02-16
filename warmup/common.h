#ifndef __COMMON_H
#define __COMMON_H

/* DO NOT CHANGE THIS FILE */

#include <stdio.h>
#include <stdlib.h>

#define TBD() do {							\
		printf("%s:%d: %s: please implement this functionality\n", \
		       __FILE__, __LINE__, __FUNCTION__);		\
		exit(1);						\
	} while (0)

#define __STR(n) #n
#define STR(n) __STR(n)

/* When you invoke a system call, ALWAYS check if it returns an error. An error
 * is returned when the return value of the system call is less than 0. For this
 * lab, on an error, call the macro shown below, with the name of the system
 * call, and the file on which the system call was invoked. For example:
 *
 * if (syscall(file, ...) < 0) {
 *         syserror(syscall, file);
 * }
 */
#define syserror(syscall, file) \
do { \
	fprintf(stderr, "%s: %s: %s\n", STR(syscall), file, strerror(errno)); \
	exit(1); \
} while (0)

#endif /* __COMMON_H */
