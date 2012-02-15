#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file_service.h"
#include "common.h"

/* set to 1 when we must exit */
sig_atomic_t done = 0;

static void exit_handler(int signo)
{
	done = 1;
}

static void pidfile_create(char *pidfile_name)
{
	FILE *pidfile;
	if (!(pidfile = fopen(pidfile_name, "w")))
	    fail_en("fopen");
	fprintf(pidfile, "%d\n", getpid());
	fclose(pidfile);
}

static void pidfile_destroy(char *pidfile_name)
{
	remove(pidfile_name);
}

/* Creates the a shared memory segment of size `size` mapped from file at
 * `fname`. `shm_flags` contains any extra flags wished to pass into
 * `shm_open() `*/
static void *_map_shm(char *fname, size_t size, int shm_flags)
{
	mode_t mode = 0666; // read+write for all
	int flags = O_RDWR | shm_flags;
	int fd = shm_open(fname, flags, mode);
	if (fd == -1)
		fail_en("shm_open");

	if (ftruncate(fd, size) == -1)
		fail_en("ftruncate");

	int prot = PROT_READ | PROT_WRITE;
	void *p = mmap(NULL, 0, prot, MAP_SHARED, fd, 0);
	if (p == (void *) -1)
		fail_en("mmap");
	close(fd);
	return p;
}

/* Creates the a shared memory segment of size `size` mapped from file at
 * `fname`. */
void *create_shm(char *fname, size_t size)
{
	/* wrap `_map_shm()`, making sure the segment is created */
	return _map_shm(fname, size, O_CREAT | O_EXCL);
}

/* Maps the a shared memory segment of size `size` from file at
 * `fname`. */
void *map_shm(char *fname, size_t size) {
	return _map_shm(fname, size, 0);
}

static void install_sig_handler(int signo, void (*handler)(int))
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handler;
	if (sigaction(signo, &sa, NULL) == -1)
		fail_en("sigaction");
}

/* Puts application into a daemon state. Keeps the working directory where
 * it is */
static void daemonize()
{
	bool change_cwd_to_root = False;
	bool close_stdout = True;
	if (daemon(!change_cwd_to_root, !close_stdout))
		fail_en("daemon");
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		fail("Must provide a pidfile path");
	daemonize();

	char *pidfile_path = argv[1];
	pidfile_create(pidfile_path);

	install_sig_handler(SIGTERM, &exit_handler);

	struct fs_registrar *reg = map_shm(shm_registrar_name, sizeof(*reg));

	while (!done)
		sleep(5);

	pidfile_destroy(pidfile_path);
	return 0;
}
