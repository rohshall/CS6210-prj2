/*
 * Funcions and data supporting the circular linked list of server threads.
 */

#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include <pthread.h>
#include <semaphore.h>


union fs_process_sring_entry;

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

/* circular linked list */
struct server_list {
	struct server_list_node *first;
	struct server_list_node *last;
	sem_t full;
	sem_t mtx;
};


/* Inits list semephores, pointers, etc. */
void server_threads_list_init(struct server_list *list);

/* Alloc's, initializes and returns a new server_list_node */
struct server_list_node *server_list_node_create();

/* Destroy's a server_list_node */
void server_list_node_destroy(struct server_list_node *n);

/* Insert a node into the list. Assumes a NULL list->first is an empty list */
void server_list_insert(struct server_list *list, struct server_list_node *n);

#endif /* end of include guard: LINKED_LIST_H_ */

