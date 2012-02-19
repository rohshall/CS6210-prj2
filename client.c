#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#include "file_service.h"

/* Registers this client with the file server. On return, the server will have
 * created a ring buffer for us to use, and we will know the sector limits for
 * the served file. */
static void register_with_server()
{
	struct fs_registrar_sring *reg = shm_map(shm_registrar_name, sizeof(*reg));
	int req = getpid();
	struct sector_limits rsp;

	RB_MAKE_REQUEST(fs_registrar, reg, &req, &rsp);

	printf("Client: requested %d, recieved (%d, %d)\n", req,
	       rsp.start, rsp.end);
	shm_unmap(reg, sizeof(*reg));
}

int main(int argc, char const *argv[])
{
	register_with_server();

	printf("hello client world\n");
	return 0;
}
