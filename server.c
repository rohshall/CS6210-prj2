#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "file_service.h"
#include "common.h"

/* set to 1 when we must exit */
volatile sig_atomic_t done = 0;

/* Node for the linked list of registered server threads */
struct server_list_node {
	struct server_list_node *next;
	union fs_process_sring_entry *entry;
	int has_work;
	pthread_mutex_t mtx;	/* protect has_work */
	pthread_cond_t cond;
	/* The following two fields are for signaling the pthread to stop */
	sem_t *sem;
	pthread_t tid;
};

/* global linked list */
struct server_list {
	struct server_list_node *list;
	sem_t full;
	sem_t mtx;
} server_list;

void server_threads_list_init(struct server_list *list)
{
	list->list = NULL;
	sem_init(&list->full, 0, 0);
	sem_init(&list->mtx, 0, 1);
}

/* Alloc's, initializes and returns a new server_list_node */
struct server_list_node *server_list_node_create()
{
	struct server_list_node *n = ecalloc(sizeof(*n));
	pthread_mutex_init(&n->mtx, NULL);
	pthread_cond_init(&n->cond, NULL);
	return n;
}

/* Destroy's a server_list_node */
void server_list_node_destroy(struct server_list_node *n)
{
	pthread_mutex_destroy(&n->mtx);
	pthread_cond_destroy(&n->cond);
	free(n);
}

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

static void data_lookup_handle(union fs_process_sring_entry *entry)
{
        int sector = entry->req;

	//char *rsp;
	sprintf(entry->rsp.data, "test");

	//entry->rsp.data = rsp;
}

static void *start_worker(void* rname)
{
        struct ring_name *rData = (struct ring_name*)rname;
        struct fs_process_sring *reg = rData->ring;
	char* shmWorkerName = rData->shm_name;

	printf("worker thread\n");

	RB_SERVE(fs_process, reg, done, &data_lookup_handle);

	shm_destroy(shmWorkerName, reg, sizeof(*reg));

	return 0;
}

/* Handles a single request/response for client registration. Takes in the
 * client pid in entry->req, and pushes the sector limits for the served file in
 * entry->rsp. */
static void reg_handle_request(union fs_registrar_sring_entry *entry)
{
	// process_request
	int client_pid = entry->req;
	printf("server: client request %d\n", client_pid);

	//create new ring
        char *shmWorkerName = shm_ring_buffer_name(client_pid);
	struct fs_process_sring *reg = shm_create(shmWorkerName, sizeof(*reg));
	RB_INIT(fs_process, reg, FS_PROCESS_SLOT_COUNT);

	struct ring_name rname;
	rname.ring = (void*)reg;
	rname.shm_name = shmWorkerName;

	//spawn new worker thread)
	pthread_t newThread;
	pthread_create(&newThread, NULL, start_worker, &rname);

	printf("thread created. worker shm %s\n", shmWorkerName);

	// push_response
	entry->rsp.start = -1 * client_pid;
	entry->rsp.end =  client_pid;
}

/* starts the infinite loop for the client registrar */
static void start_registrar()
{
	/* Create the internal circular linked list for worker threads */
	server_threads_list_init(&server_list);

	/* Create the registration ring buffer for clients */
	struct fs_registrar_sring *reg = shm_create(shm_registrar_name,
						    sizeof(*reg));
	RB_INIT(fs_registrar, reg, FS_REGISTRAR_SLOT_COUNT);
	RB_SERVE(fs_registrar, reg, done, &reg_handle_request);
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
