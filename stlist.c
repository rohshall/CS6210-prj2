/*
 * Functions supporting the circular linked list.
 */

#include <pthread.h>
#include <semaphore.h>

#include "stlist.h"
#include "common.h"
#include "file_service.h"

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

void stlist_init(struct stlist *list)
{
	/* A dummy sentinal node. The list is empty when first==nil */
	list->nil = stlist_node_create();
	list->first = list->nil;
	sem_init(&list->full, 0, 0);
	sem_init(&list->mtx, 0, 1);
}

bool stlist_is_empty(struct stlist *list) {
	return list->first == list->nil;
}

/* Insert a node into the list. Assumes list->first==list->nil is an empty list */
void stlist_insert(struct stlist *list, struct stlist_node *n)
{
	sem_wait(&list->mtx);
	if (stlist_is_empty(list)) {
		list->first = n;
		n->next = n;
		list->nil->next = n;
	} else {
		n->next = list->first->next;
		list->first->next = n;
	}
	sem_post(&list->mtx);
}
