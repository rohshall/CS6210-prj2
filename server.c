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

/* Handles a single ring buffer request/repsonse sequence. Currently specific
 * to registration ring buffers */
static void rb_handle_request(int slot_index, struct fs_registration *slot)
{
	pthread_mutex_lock(&slot->mutex);

	// pull_request();
	int pid = slot->client_pid;

	// process_request();
	printf("server %d client %d\n", slot_index, pid);

	// push_response();
	slot->client_pid *= -1;

	slot->done = 1;
	pthread_cond_signal(&slot->condvar);
	while (slot->done)
		pthread_cond_wait(&slot->condvar, &slot->mutex);
	pthread_mutex_unlock(&slot->mutex);
}

/* Initializes a ring buffer. Currently specific to registration ring buffers */
static void rb_init(struct fs_registrar *reg)
{
	sem_init(&(reg->empty), 1, FS_REGISTRAR_SLOT_COUNT);
	sem_init(&(reg->full), 1, 0);
	sem_init(&(reg->mtx), 1, 1);
	reg->client_index = 0;

	pthread_mutexattr_t m_attr;
	pthread_mutexattr_init(&m_attr);
	pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_t c_attr;
	pthread_condattr_init(&c_attr);
	pthread_condattr_setpshared(&c_attr, PTHREAD_PROCESS_SHARED);
	struct fs_registration *slot;
	for (int i = 0; i < FS_REGISTRAR_SLOT_COUNT; ++i) {
		slot = &reg->registrar[i];
		pthread_mutex_init(&slot->mutex, &m_attr);
		pthread_cond_init(&slot->condvar, &c_attr);
		slot->done = 0;
	}
}

static void start_registrar()
{
	struct fs_registrar *reg = shm_create(shm_registrar_name, sizeof(*reg));
	rb_init(reg);

	struct fs_registration *slot;
	int server_index = 0;
	while (!done) {
		if (sem_wait(&(reg->full)) == -1) {
			int en = errno;
			if (en == EINTR)
				continue;
		}

		slot = &reg->registrar[server_index];
		rb_handle_request(server_index, slot);

		sem_post(&(reg->empty));
		server_index = (server_index + 1) % FS_REGISTRAR_SLOT_COUNT;
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
