/*
 * Functions supporting the circular linked list.
 */

#include <pthread.h>
#include <semaphore.h>

#include "stlist.h"
#include "common.h"
#include "file_service.h"

void stlist_init(struct stlist *list)
{
	list->first = NULL;
	list->last = NULL;
	sem_init(&list->full, 0, 0);
	sem_init(&list->mtx, 0, 1);
}

/* Alloc's, initializes and returns a new server_list_node */
struct stlist_node *stlist_node_create()
{
	struct stlist_node *n = ecalloc(sizeof(*n));
	pthread_mutex_init(&n->mtx, NULL);
	pthread_cond_init(&n->cond, NULL);
	return n;
}

/* Destroy's a server_list_node */
void stlist_node_destroy(struct stlist_node *n)
{
	pthread_mutex_destroy(&n->mtx);
	pthread_cond_destroy(&n->cond);
	free(n);
}

/* Insert a node into the list. Assumes a NULL list->first is an empty list */
void stlist_insert(struct stlist *list, struct stlist_node *n)
{
	sem_wait(&list->mtx);
	if (!list->first) {
		list->first = n;
		list->last = n;
		n->next = n;
	} else {
		n->next = list->first;
		list->last->next = n;
		list->first = n;
	}
	sem_post(&list->mtx);
}
