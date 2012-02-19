#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

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

static void reg_handle_request(union fs_registrar_sring_entry *entry)
{
	// process_request();
	int client_pid = entry->req;
	printf("server: client request %d\n", client_pid);

	// push_response();
	entry->rsp.start = -1 * client_pid;
	entry->rsp.end =  client_pid;

}

/* Handles a single ring buffer request/repsonse sequence. Currently specific
 * to registration ring buffers */
static void
rb_handle_request(struct fs_registrar_sring_slot *slot,
		  void (*request_handler)(union fs_registrar_sring_entry *))
{
	pthread_mutex_lock(&slot->mutex);

	request_handler(&slot->entry);

	slot->done = 1;
	pthread_cond_signal(&slot->condvar);
	while (slot->done)
		pthread_cond_wait(&slot->condvar, &slot->mutex);
	pthread_mutex_unlock(&slot->mutex);
}

static void start_registrar()
{
	struct fs_registrar_sring *reg = shm_create(shm_registrar_name,
						    sizeof(*reg));
	RB_INIT(fs_registrar, reg, FS_REGISTRAR_SLOT_COUNT);

	struct fs_registrar_sring_slot *slot;
	int server_index = 0;
	while (!done) {
		if (sem_wait(&(reg->full)) == -1) {
			int en = errno;
			if (en == EINTR)
				continue;
		}

		slot = &reg->ring[server_index];
		rb_handle_request(slot, &reg_handle_request);

		sem_post(&(reg->empty));
		server_index = (server_index + 1) % reg->slot_count;
	}
	shm_destroy(shm_registrar_name, reg, sizeof(*reg));
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		fail("Must provide a pidfile path");
	daemonize();

	char *pidfile_path = argv[1];
	pidfile_create(pidfile_path);

	install_sig_handler(SIGTERM, &exit_handler);

	start_registrar();

	pidfile_destroy(pidfile_path);
	return 0;
}
