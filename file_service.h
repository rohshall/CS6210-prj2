/*
 * This header is included by both the server and the client. It should define
 * the interface between the two, and not contain anything private to either
 * module
 */

#ifndef FILE_SERVICE_H_
#define FILE_SERVICE_H_

#include <semaphore.h>
#include "ring.h"

/* Name of the shared memory file that clients should `mmap()` to register
 * their pid with the server */
#define shm_registrar_name "/fs_registrar"

/* types for the request/response for the registration buffer */
typedef int client_pid_t;

typedef struct sector_limits {
	int start;
	int end;
} sector_limits_t;

/* number of slots in the ringbuffer */
#define FS_REGISTRAR_SLOT_COUNT 10

/* defines the request/response union, the entry slot struct, and the ringbuffer
 * struct */
DEFINE_RING_TYPES(fs_registrar, client_pid_t, sector_limits_t,
		  FS_REGISTRAR_SLOT_COUNT);

/* Prefix of the name of the shared memory file that clients should `mmap()` to
 * communicate via ring buffer. The full name will be
 * "shm_ring_buffer_prefix.pid", where `pid` is the PID of the client.
 */
#define shm_ring_buffer_prefix "/fs_ringbuffer"

/* Functions to handle shared memory */
/* create and map a new shared memory segement */
void *shm_create(char *fname, size_t size);
/* map an exisiting shared memory segment */
void *shm_map(char *fname, size_t size);
/* unmap a shm segment, but don't delete it */
void shm_unmap(void *ptr, size_t size);
/* unmap and destroy a shared memory segment */
void shm_destroy(char *fname, void *ptr, size_t size);

#endif /* end of include guard: FILE_SERVICE_H_ */
