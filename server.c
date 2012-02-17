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
	sa.sa_flags = 0;
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

void registrar(struct fs_registrar *reg)
{
  sem_init(&(reg->empty), 1, FS_REGISTRAR_SLOT_COUNT);
  sem_init(&(reg->full), 1, 0);
  sem_init(&(reg->mtx), 1, 1);
  reg->client_index = 0;

  int server_index = 0;
  while(server_index < 10){

    int pid = 0;
    
    sem_wait(&(reg->full));
    sem_wait(&(reg->mtx));

    pid = reg->registrar[server_index].client_pid;
    
    sem_post(&(reg->mtx));
    sem_post(&(reg->empty));

    //process pid;
    printf("server %d client %d", server_index, pid);
    
    kill(pid, SIGUSR1);

    server_index++;
  }
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

	registrar(reg);

	while (!done)
		sleep(5);
	

	shm_destroy(shm_registrar_name, reg, sizeof(*reg));
	pidfile_destroy(pidfile_path);
	return 0;
}
