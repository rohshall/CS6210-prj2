#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#include "file_service.h"


volatile sig_atomic_t ready = 0;

void sigusr_handler(int signo){
  ready = 1;
}

static void install_sig_handler(int signo, void (*handler)(int))
{
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigusr_handler;
	if (sigaction(signo, &sa, NULL) == -1)
	  exit(1);
}

static void register_with_server()
{
	struct fs_registrar *reg = shm_map(shm_registrar_name, sizeof(*reg));
	sem_wait(&(reg->empty));
	sem_wait(&(reg->mtx));

	struct fs_registration *slot = &reg->registrar[reg->client_index];
	int request = getpid();
	slot->req_resp.client_pid = request;
	slot->done = 0;
	reg->client_index = (reg->client_index + 1) % FS_REGISTRAR_SLOT_COUNT;

	sem_post(&(reg->mtx));
	sem_post(&(reg->full));

	pthread_mutex_lock(&slot->mutex);
	while(!slot->done)
		pthread_cond_wait(&slot->condvar, &slot->mutex);
	int sector_start = slot->req_resp.sector_limits[0];
	int sector_end = slot->req_resp.sector_limits[1];
	slot->done = 0;
	pthread_cond_signal(&slot->condvar);
	pthread_mutex_unlock(&slot->mutex);

	shm_unmap(reg, sizeof(*reg));

	printf("Client: requested %d, recieved (%d, %d)\n", request,
	       sector_start, sector_end);
}

int main(int argc, char const *argv[])
{
	install_sig_handler(SIGUSR1, &sigusr_handler);
	register_with_server();

	printf("hello client world\n");
	return 0;
}
