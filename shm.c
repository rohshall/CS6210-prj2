/*
 * Functions relating to creating and destroying shared memory
 *
 */

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"

/* Creates the a shared memory segment of size `size` mapped from file at
 * `fname`. `shm_flags` contains any extra flags wished to pass into
 * `shm_open() `*/
static void *_shm_create_and_map(char *fname, size_t size, int shm_flags)
{
	mode_t mode = 0666; // read+write for all
	int flags = O_RDWR | shm_flags;
	int fd = shm_open(fname, flags, mode);
	if (fd == -1)
		fail_en("shm_open");

	if (ftruncate(fd, size) == -1)
		fail_en("ftruncate");

	int prot = PROT_READ | PROT_WRITE;
	void *p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
	if (p == (void *) -1)
		fail_en("mmap");
	close(fd);
	return p;
}

/* Creates the a shared memory segment of size `size` mapped from file at
 * `fname`. */
void *shm_create(char *fname, size_t size)
{
	/* wrap `_map_shm()`, making sure the segment is created */
	return _shm_create_and_map(fname, size, O_CREAT | O_EXCL);
}

/* Maps the a shared memory segment of size `size` from file at
 * `fname`. */
void *shm_map(char *fname, size_t size)
{
	return _shm_create_and_map(fname, size, 0);
}

/* Unmaps the shared memory. Does not delete the underlying segment */
void shm_unmap(void *p, size_t size)
{
	if (munmap(p, size) == -1)
		fail_en("munmap");
}

/* munmaps and unlinks the shared memory */
void shm_destroy(char *fname, void *p, size_t size)
{
	shm_unmap(p, size);
	if (shm_unlink(fname) == -1)
		fail_en("shm_unlink");
}
