#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>

#include "file_service.h"
#include "common.h"

/**
   struct to pass data from main thread to worker thread
 */
struct client_worker_data{
  struct fs_process_sring *ring;
  struct sector_limits *limits;
  int numOfRequest;
  struct client_worker_result *result;
};

/** 
    struct to store data of each response received 
*/
struct client_worker_result{
  long time;
  int sectorNum;
  struct sector_data data;
};

/**
   calculate average time from client_worker_result
*/
static double getTimeAvg(struct client_worker_result * result, int numOfItem){
	int i = 0;
	long total = 0;
	
	for(i = 0; i<numOfItem; i++){
	  total = total + result[i].time;
	}
	
	return (double)total/numOfItem;
}

/** 
   find max time from client_worker_result
*/
static long getTimeMax(struct client_worker_result * result, int numOfItem){
        int i = 0;
	long max = result[i].time;
	
	for(i = 0; i<numOfItem; i++){
	  if(max < result[i].time) {
	    max = result[i].time;
	  }
	}
	return max;
}

/** 
   find min time from client_worker_result
*/
static long getTimeMin(struct client_worker_result * result, int numOfItem){
        int i = 0;
	long min = result[i].time;
	
	for(i = 0; i<numOfItem; i++){
	  if(min > result[i].time) {
	    min = result[i].time;
	  }
	}
	return min;
}

/** 
   calculate standard deviation of client_worker_result time data 
*/
static double getTimeStdDev(struct client_worker_result *result, int numOfItem, double avg) {
        int i;
	double diffSq[numOfItem];
	double stdDev;
	double total = 0;
	for(i = 0; i<numOfItem; i++){
	  diffSq[i] = pow((result[i].time - avg), 2);
	}
	
	for(i=0; i<numOfItem; i++){
	  total = total + diffSq[i];
	}

	stdDev = sqrt(total/numOfItem);
	
	return stdDev;
}

/**
   write client_worker_result data to 2 files:
   read.<client_pid> -> sector numbers, line break separated
   write.<client_pid> -> binary file, data from server
 */
static void writeResult(struct client_worker_result *result, int numOfItem){
        FILE *fpRead, *fpSector;
	char fReadName[60], fSectorName[60];
	int i;
	
	sprintf(fReadName, "./read.%d", getpid());
	sprintf(fSectorName, "./sector.%d", getpid());
	
	fpRead = fopen(fReadName, "w+");
	fpSector = fopen(fSectorName, "w+b");
	
	for(i=0; i<numOfItem; i++){
	  fwrite(result[i].data.data, sizeof(result[i].data.data[0]), sizeof(result[i].data.data)/sizeof(result[i].data.data[0]), fpSector);

	  fprintf(fpRead, "%d\n", result[i].sectorNum);
	}

	fclose(fpRead);
	fclose(fpSector);
}

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
	struct client_worker_result *result = ((struct client_worker_data *)workerData)->result;

	int i;
	for(i=0; i<numOfRequest; i++){
	        struct sector_data rsp;
		int req = (rand() % (sector->end-sector->start)) + sector->start;
		struct timespec tpStart, tpEnd;
		
		clock_gettime(CLOCK_REALTIME, &tpStart);
	        RB_MAKE_REQUEST(fs_process, ring, &req, &rsp);
		clock_gettime(CLOCK_REALTIME, &tpEnd);

		result[i].data = rsp;		
		result[i].time = tpEnd.tv_nsec - tpStart.tv_nsec;
		result[i].sectorNum = req;
		//printf("Client: requested %d, received %s\n", req, result[i].data.data);

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

	int requestPerThread = (int)numOfRequest/numOfThread;

	struct client_worker_result result[numOfRequest];
	struct client_worker_data clientData[numOfThread];

	//init pthread
	pthread_t workerThread[numOfThread];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	int i;
	for(i=0; i<numOfThread; i++){

	        clientData[i].ring = ring;
		clientData[i].limits = &sector;
		clientData[i].numOfRequest = requestPerThread;
	        if((numOfRequest%numOfThread != 0) && (i == numOfThread-1)){
		  clientData[i].numOfRequest = requestPerThread + numOfRequest%numOfThread;
		}
		clientData[i].result = &result[i*clientData[i].numOfRequest];



	        pthread_create(&workerThread[i], &attr, request_worker, &clientData[i]);

	}

	for(i=0; i<numOfThread; i++){
	        pthread_join(workerThread[i], NULL);
	}

	//process result
	for(i = 0; i < numOfRequest; i++) {
	  printf("result %d: time: %ld data: %s\n", i, result[i].time, result[i].data.data);
	  
	}
	double avg = getTimeAvg(result, numOfRequest);
	long max = getTimeMax(result, numOfRequest);
	long min = getTimeMin(result, numOfRequest);
	double stddev = getTimeStdDev(result, numOfRequest, avg);
	printf("%ld|%f|%ld|%f\n", max, avg, min, stddev);

	//write to file
	writeResult(result, numOfRequest);

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
