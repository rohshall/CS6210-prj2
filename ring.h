
#ifndef RING_H_
#define RING_H_



/* Defines the types for the ring buffer */
#define DEFINE_RING_TYPES(_tag, __req_t, __rsp_t, __ring_size)		\
typedef __req_t _tag##_req_t;						\
typedef __rsp_t _tag##_rsp_t;						\
union _tag##_sring_entry {						\
	_tag##_req_t req;						\
	_tag##_rsp_t rsp;						\
};									\
									\
struct _tag##_sring_slot {						\
	pthread_mutex_t mutex;						\
	pthread_cond_t condvar;						\
	int done;							\
	union _tag##_sring_entry entry;					\
};									\
									\
struct _tag##_sring {							\
	sem_t empty;							\
	sem_t full;							\
	sem_t mtx;							\
	int client_index;						\
	int slot_count;							\
	struct _tag##_sring_slot ring[__ring_size];			\
}


/* For the client, makes a request and blocks, waiting for a response. The
 * response is copied to rsp_ptr */
#define MAKE_REQUEST(_tag, _ring, _req_ptr, _rsp_ptr) do {		\
	sem_wait(&(_ring)->empty);					\
	sem_wait(&(_ring)->mtx);					\
									\
	struct _tag##_sring_slot *slot = 				\
		&(_ring)->ring[(_ring)->client_index];			\
	slot->entry.req = *(_req_ptr);					\
	slot->done = 0;							\
	(_ring)->client_index = ((_ring)->client_index + 1) %		\
			(_ring)->slot_count;				\
									\
	sem_post(&((_ring)->mtx));					\
	sem_post(&((_ring)->full));					\
									\
	pthread_mutex_lock(&slot->mutex);				\
	while(!slot->done)						\
		pthread_cond_wait(&slot->condvar, &slot->mutex);	\
	*(_rsp_ptr) = slot->entry.rsp;					\
	slot->done = 0;							\
	pthread_cond_signal(&slot->condvar);				\
	pthread_mutex_unlock(&slot->mutex);				\
} while (0)

#endif /* end of include guard: RING_H_ */
