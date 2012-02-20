/*
 * common.h
 *
 * Useful macros, etc
 *
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef int bool;
#define False 0
#define True 1

#define fail_en(msg) do { \
	perror(msg); \
	exit(EXIT_FAILURE); \
} while (0)

#define fail(msg) do { \
	fprintf(stderr, "Error: %s\n", msg); \
	exit(EXIT_FAILURE); \
} while (0)

/* check pointing: compiler always checks that the code is valid, but will
 * optimize it out if DEBUG not defined. Requires C99 for the varaible args */
#ifdef DEBUG
#define DEBUG_PRINT 1
#else
#define DEBUG_PRINT 0
#endif
#define checkpoint(fmt, ...) do { \
	if (DEBUG_PRINT) \
		fprintf(stderr, "%-25s%5d:%35s(): " fmt "\n", __FILE__ ":", \
			__LINE__, __func__, __VA_ARGS__); \
} while (0)


/* *alloc's that fail */
static inline void *emalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
		fail("malloc");
	return ptr;
}

static inline void *ecalloc(size_t size)
{
	void *ptr = calloc(1, size);
	if (!ptr)
		fail("calloc");
	return ptr;
}


#endif /* COMMON_H_ */
