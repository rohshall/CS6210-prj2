#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "file_service.h"
#include "common.h"

/* set to 1 when we must exit */
volatile sig_atomic_t done = 0;

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

static void install_sig_handler(int signo, void (*handler)(int))
{
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.flags = 0;
	sa.sa_handler = handler;
	if (sigaction(signo, &sa, NULL) == -1)
		fail_en("sigaction");
}

/* Puts application into a daemon state. Keeps the working directory where
 * it is */
static void daemonize()
{
	bool change_cwd_to_root = False;
	bool close_stdout = False; // maybe set to True once debugged
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

	struct fs_registrar *reg = shm_create(shm_registrar_name,
					      sizeof(*reg));

	while (!done)
		sleep(5);

	shm_destroy(shm_registrar_name, reg, sizeof(*reg));
	pidfile_destroy(pidfile_path);
	return 0;
}
