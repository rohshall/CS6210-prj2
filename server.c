#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "file_service.h"
#include "common.h"
#include "stlist.h"

/* ring ptr and shm_name pair for server worker threads*/
struct ring_name {
        struct fs_process_sring* ring;
        char* shm_name;
	struct stlist_node *ll_node;
}ring_name_t;

/* global circular linked list */
struct stlist server_list;

/* set to 1 when we must exit */
volatile sig_atomic_t done = 0;

/* Sig handler for exit signal */
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

/* Main file server thread. Continually waits for work to be put in the cicular
 * linked list of worker threads. When there is work to be done, it loops around
 * the list taking each job in round-robin fasion. It performs each job and
 * signals to the worker thread that it is done.
 */
static void file_server()
{
	sem_wait(&server_list.mtx);
	struct stlist_node *p = server_list.first;
	sem_post(&server_list.mtx);
	int has_work = False;
	while (1) {
		sem_wait(&server_list.full);
		// Loop around, finding the first node that has work to be done
		sem_wait(&server_list.mtx);
		do {
			p = p->next;
			pthread_mutex_lock(&p->mtx);
			has_work = p->has_work;
			pthread_mutex_unlock(&p->mtx);
		} while (!has_work);
		sem_post(&server_list.mtx);
	}
}

/* Handler for the worker thread servers. Note that the fs_process_sring_entry
 * is locked by a mutex through the duration of this call */
static void data_lookup_handle(union fs_process_sring_entry *entry,
			       struct stlist_node *ll_node)
{
	/* We let the file server thread we have work to do, and wait till it
	 * finishes */
	pthread_mutex_lock(&ll_node->mtx);
	ll_node->entry = entry;
	ll_node->has_work = 1;
	sem_post(&server_list.full);
	while (!ll_node->has_work)
		pthread_cond_wait(&ll_node->cond, &ll_node->mtx);
	pthread_mutex_unlock(&ll_node->mtx);

	/*
        int sector = entry->req;

	//char *rsp;
	sprintf(entry->rsp.data, "test");

	//entry->rsp.data = rsp;
	*/
}

static void *start_worker(void* rname)
{
        struct ring_name *rData = (struct ring_name*)rname;
        struct fs_process_sring *reg = rData->ring;
	char* shmWorkerName = rData->shm_name;
	struct stlist_node *ll_node = rData->ll_node;

	printf("worker thread\n");

	RB_SERVE(fs_process, reg, done, &data_lookup_handle, ll_node);

	shm_destroy(shmWorkerName, reg, sizeof(*reg));

	return 0;
}

/* Handles a single request/response for client registration. Takes in the
 * client pid in entry->req, and pushes the sector limits for the served file in
 * entry->rsp. */
static void reg_handle_request(union fs_registrar_sring_entry *entry, void *nil)
{
	// process_request
	int client_pid = entry->req;
	printf("server: client request %d\n", client_pid);

	//create new ring
        char *shmWorkerName = shm_ring_buffer_name(client_pid);
	struct fs_process_sring *reg = shm_create(shmWorkerName, sizeof(*reg));
	RB_INIT(fs_process, reg, FS_PROCESS_SLOT_COUNT);

	//create new linked list node
	struct stlist_node *n = stlist_node_create();
	n->sem = &reg->full;
	stlist_insert(&server_list, n);

	//spawn new worker thread)
	struct ring_name rname;
	rname.ring = reg;
	rname.shm_name = shmWorkerName;
	rname.ll_node = n;
	pthread_create(&n->tid, NULL, start_worker, &rname);

	printf("thread created. worker shm %s\n", shmWorkerName);

	// push_response
	entry->rsp.start = -1 * client_pid;
	entry->rsp.end =  client_pid;
}

/* starts the infinite loop for the client registrar */
static void start_registrar()
{
	/* Create the internal circular linked list for worker threads */
	stlist_init(&server_list);

	/* Create the registration ring buffer for clients */
	struct fs_registrar_sring *reg = shm_create(shm_registrar_name,
						    sizeof(*reg));
	RB_INIT(fs_registrar, reg, FS_REGISTRAR_SLOT_COUNT);
	RB_SERVE(fs_registrar, reg, done, &reg_handle_request, NULL);
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
