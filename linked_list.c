/*
 * Functions supporting the circular linked list.
 */

#include <pthread.h>
#include <semaphore.h>

#include "linked_list.h"
#include "common.h"
#include "file_service.h"

void server_threads_list_init(struct server_list *list)
{
	list->first = NULL;
	list->last = NULL;
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

/* Insert a node into the list. Assumes a NULL list->first is an empty list */
void server_list_insert(struct server_list *list, struct server_list_node *n)
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
