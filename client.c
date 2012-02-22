#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#include "file_service.h"
#include "common.h"

/* Registers this client with the file server. On return, the server will have
 * created a ring buffer for us to use, and we will know the sector limits for
 * the served file. */
static struct sector_limits register_with_server()
{
	struct fs_registrar_sring *reg = shm_map(shm_registrar_name, sizeof(*reg));
	int req = getpid();
	struct sector_limits rsp;

	RB_MAKE_REQUEST(fs_registrar, reg, &req, &rsp);

	checkpoint("Client Reg: requested %d, recieved (%d, %d)", req,
		   rsp.start, rsp.end);
	shm_unmap(reg, sizeof(*reg));

	return rsp;
}

void request_data(struct sector_limits sector)
{
        char shmWorkerName[50];
	sprintf(shmWorkerName, "%s.%d", shm_ring_buffer_prefix, getpid());
	struct fs_process_sring *ring = shm_map(shmWorkerName, sizeof(*ring));

	int req = rand() % (sector.end-sector.start) + sector.start;
	struct sector_data rsp;

	for (int i = 0; i < 100; ++i) {
		req++;
	RB_MAKE_REQUEST(fs_process, ring, &req, &rsp);

	printf("Client: requested %d, received %s\n", req, rsp.data);
	}

	shm_unmap(ring, sizeof(*ring));
}

int main(int argc, char const *argv[])
{
	struct sector_limits rsp = register_with_server();
	checkpoint("%s", "Client: done reg");
	request_data(rsp);
	return 0;
}
