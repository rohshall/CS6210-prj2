#include <stdio.h>
#include <unistd.h>
#include <signal.h>

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

	reg->registrar[reg->client_index].client_pid = getpid();
	reg->client_index++;

	sem_post(&(reg->mtx));
	sem_post(&(reg->full));

	while(!ready)
		sleep(1);
}

int main(int argc, char const *argv[])
{
	install_sig_handler(SIGUSR1, &sigusr_handler);
	register_with_server();

	printf("hello client world\n");
	return 0;
}
