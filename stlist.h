/*
 * "STLIST" = "Server Threads (Linked) List"
 * Funcions and data supporting the circular linked list of server threads.
 */

#ifndef STLIST_H_
#define STLIST_H_

#include <pthread.h>
#include <semaphore.h>

#include "common.h"

union fs_process_sring_entry;

/* Node for the linked list of registered server threads */
struct stlist_node {
	struct stlist_node *next;
	union fs_process_sring_entry *entry;
	int has_work;
	pthread_mutex_t mtx;		// protects has_work
	pthread_cond_t cond;
	pthread_t tid;			// use to cancel() the pthread
};

/* circular linked list */
struct stlist {
	struct stlist_node *first;
	struct stlist_node *nil;	// used for implementation
	sem_t full;			// 0 when there is no work to be done
	sem_t mtx;			// protects the "first" pointer and all
					// the node->next pointers
};


/* Inits list semephores, pointers, etc. */
void stlist_init(struct stlist *list);

/* Alloc's, initializes and returns a new stlist_node */
struct stlist_node *stlist_node_create();

/* Destroys a server_list_node */
void stlist_node_destroy(struct stlist_node *n);

/* Destroys all the nodes in a list. (does not "free" the list pointer) */
void stlist_destroy(struct stlist *list);

/* Insert a node into the list. Assumes a NULL list->first is an empty list */
void stlist_insert(struct stlist *list, struct stlist_node *n);

/* Returns true if list is emtpy */
bool stlist_is_empty(struct stlist *list);

#endif /* end of include guard: STLIST_H_ */

