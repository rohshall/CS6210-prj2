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
  struct timespec startTime;
  struct timespec endTime;
  struct timespec time;
  int sectorNum;
  struct sector_data data;
};

int timespec_subtract (struct timespec *result, struct timespec *start, struct timespec *end)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (end->tv_nsec < start->tv_nsec) {
    int numOfSec = (start->tv_nsec - end->tv_nsec) / 1000000000 + 1;
    start->tv_nsec -= 1000000000 * numOfSec;
    start->tv_sec += numOfSec;
  }
  if (end->tv_nsec - start->tv_nsec > 1000000000) {
    int numOfSec = (end->tv_nsec - start->tv_nsec) / 1000000000;
    start->tv_nsec += 1000000000 * numOfSec;
    start->tv_sec -= numOfSec;
  }
  
  /* Compute the time remaining to wait.
     tv_nsec is certainly positive. */
  result->tv_sec = end->tv_sec - start->tv_sec;
  result->tv_nsec = end->tv_nsec - start->tv_nsec;
  
  /* Return 1 if result is negative. */
  return end->tv_sec < start->tv_sec;
}

void timespec_add(struct timespec *result, struct timespec *ts1, struct timespec *ts2){
  result->tv_sec = ts1->tv_sec + ts2->tv_sec;
  result->tv_nsec = ts1->tv_nsec + ts2->tv_nsec;
  if(result->tv_nsec >= 1000000000){
    int numOfSec = result->tv_nsec / 1000000000;
    result->tv_nsec -= 1000000000 *numOfSec;
    result->tv_sec += numOfSec;
  }
}

/**
   returns:
   1 if ts1 > ts2
   -1 if ts1 < ts2
   0 if ts1 = ts2
 */
int timespec_compare(struct timespec *ts1, struct timespec *ts2){
  if(ts1->tv_sec > ts2->tv_sec){
    return 1;
  }
  else if(ts1->tv_sec < ts2->tv_sec){
    return -1;
  }
  else{
    if(ts1->tv_nsec > ts2->tv_nsec){
      return 1;
    }
    else if(ts1->tv_nsec < ts2->tv_nsec){
      return -1;
    }
    else{
      return 0;
    }
  }
}

long convertToNanoSec(struct timespec *ts){
  long result = ts->tv_sec*1000000000;
  result = result + ts->tv_nsec;
  return result;
}

/**
   calculate average time from client_worker_result
*/
static double getTimeAvg(struct client_worker_result * result, int numOfItem){
	int i = 0;
	struct timespec total;
	total.tv_sec = 0;
	total.tv_nsec=0;

	for(i = 0; i<numOfItem; i++){
	  timespec_add(&total, &total, &result[i].time);
	}
	
	return (double)convertToNanoSec(&total)/numOfItem;
}

/** 
   find max time from client_worker_result
*/
static long getTimeMax(struct client_worker_result * result, int numOfItem){
        int i = 0;
	struct timespec max = result[i].time;
	
	for(i = 0; i<numOfItem; i++){
	  if(timespec_compare(&max, &result[i].time) == -1) {
	    max = result[i].time;
	  }
	}
	return (long)convertToNanoSec(&max);
}

/** 
   find min time from client_worker_result
*/
static long getTimeMin(struct client_worker_result * result, int numOfItem){
        int i = 0;
	struct timespec min = result[i].time;
	
	for(i = 0; i<numOfItem; i++){
	  if(timespec_compare(&min, &result[i].time) == 1) {
	    min = result[i].time;
	  }
	}
	return (long)convertToNanoSec(&min);
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
	  diffSq[i] = pow((convertToNanoSec(&result[i].time) - avg), 2);
	}
	
	for(i=0; i<numOfItem; i++){
	  total = total + diffSq[i];
	}

	stdDev = sqrt(total/numOfItem);
	
	return stdDev;
}

static double getReqPerSec(struct client_worker_result *result, int numOfItem){
  struct timespec diff;
  timespec_subtract(&diff, &result[0].startTime, &result[numOfItem-1].endTime);
  return numOfItem/(convertToNanoSec(&diff)/1000000000.);
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
	sprintf(fSectorName, "./sectors.%d", getpid());
	
	fpRead = fopen(fReadName, "w+");
	fpSector = fopen(fSectorName, "w+b");
	
	for(i=0; i<numOfItem; i++){
	  fwrite(result[i].data.data, sizeof(result[i].data.data[0]), sizeof(result[i].data.data)/sizeof(result[i].data.data[0]), fpRead);

	  fprintf(fpSector, "%d\n", result[i].sectorNum);
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
void *request_worker(void *arg){
	struct client_worker_data *workerData = arg;
	struct fs_process_sring *ring = workerData->ring;
	struct sector_limits *sector = workerData->limits;
	int numOfRequest = workerData->numOfRequest;
	struct client_worker_result *result = workerData->result;

	int i;
	for(i=0; i<numOfRequest; i++){
		result[i].sectorNum = (rand() % (sector->end-sector->start)) + sector->start;

		//clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tpStart);
		clock_gettime(CLOCK_MONOTONIC, &(result[i].startTime));
		RB_MAKE_REQUEST(fs_process, ring, &result[i].sectorNum,
				&result[i].data);
		//clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tpEnd);
		clock_gettime(CLOCK_MONOTONIC, &(result[i].endTime));

		timespec_subtract(&result[i].time, &result[i].startTime, &result[i].endTime);
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

	struct client_worker_result *result = emalloc(numOfRequest *
						      sizeof(*result));
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
		clientData[i].result = &result[i*requestPerThread];



	        pthread_create(&workerThread[i], &attr, request_worker, &clientData[i]);

	}

	for(i=0; i<numOfThread; i++){
	        pthread_join(workerThread[i], NULL);
	}

	//calculate measurements
	double avg = getTimeAvg(result, numOfRequest);
	long max = getTimeMax(result, numOfRequest);
	long min = getTimeMin(result, numOfRequest);
	double stddev = getTimeStdDev(result, numOfRequest, avg);
	double reqPerSec = getReqPerSec(result, numOfRequest);
	printf("%ld|%f|%ld|%f|%f\n", max, avg, min, stddev, reqPerSec);

	//write to file
	writeResult(result, numOfRequest);

	pthread_attr_destroy(&attr);
	shm_unmap(ring, sizeof(*ring));
	free(result);
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
