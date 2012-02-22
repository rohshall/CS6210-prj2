#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#include "file_service.h"
#include "common.h"

/**
   struct to pass data from main thread to worker thread
 */
struct client_worker_data{
  struct fs_process_sring *ring;
  struct sector_limits *limits;
  int numOfRequest;
};

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

/**
    Client worker thread
    Generate random number within the limits. Put in a read request, then
    prints out the received data.
 **/
void *request_worker(void * workerData){
        struct fs_process_sring *ring = ((struct client_worker_data *)workerData)->ring;
	struct sector_limits *sector = ((struct client_worker_data *)workerData)->limits;
	int numOfRequest = ((struct client_worker_data *)workerData)->numOfRequest;

	int i;
	for(i=0; i<numOfRequest; i++){
	  	struct sector_data rsp;
		int req = (rand() % (sector->end-sector->start)) + sector->start;
	        RB_MAKE_REQUEST(fs_process, ring, &req, &rsp);
		printf("Client: requested %d, received %s\n", req, rsp.data);
	}
	return NULL;
}

/**
   connect the client to its own ring buffer. Then spawn off worker threads to
   do work on the shared ring buffer.
 */
void request_data(struct sector_limits sector, int numOfThread, int numOfRequest)
{
        char shmWorkerName[50];
	sprintf(shmWorkerName, "%s.%d", shm_ring_buffer_prefix, getpid());
	struct fs_process_sring *ring = shm_map(shmWorkerName, sizeof(*ring));
	/**
	int req = rand() % (sector.end-sector.start) + sector.start;
	struct sector_data rsp;

	for (int i = 0; i < 100; ++i) {
		req++;
	RB_MAKE_REQUEST(fs_process, ring, &req, &rsp);

	printf("Client: requested %d, received %s\n", req, rsp.data);
	*/

	int requestPerThread = (int)numOfRequest/numOfThread;

	struct client_worker_data clientData;
	clientData.ring = ring;
	clientData.limits = &sector;
	clientData.numOfRequest = requestPerThread;

	pthread_t workerThread[numOfThread];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	int i;
	for(i=0; i<numOfThread; i++){
	        if((numOfRequest%numOfThread != 0) && (i == numOfThread-1)){
		  clientData.numOfRequest = requestPerThread + numOfRequest%numOfThread;
		}
	        pthread_create(&workerThread[i], &attr, request_worker, &clientData);

	}

	for(i=0; i<numOfThread; i++){
	        pthread_join(workerThread[i], NULL);
	}

	pthread_attr_destroy(&attr);
	shm_unmap(ring, sizeof(*ring));
}

int main(int argc, char const *argv[])
{
        if(argc<3){
	        printf("client <num_of_threads> <num_of_requests for client>\n");
		return 0;
	}

	struct sector_limits rsp = register_with_server();
	request_data(rsp, atoi(argv[1]), atoi(argv[2]));
	return 0;
}
