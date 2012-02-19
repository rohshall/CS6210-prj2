
#ifndef RING_H_
#define RING_H_



/* Defines the types for the ring buffer */
#define DEFINE_RING_TYPES(_tag, __req_t, __rsp_t, _slot_count)		\
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
	struct _tag##_sring_slot ring[_slot_count];			\
}


/* Initialize a given ring. `_tag` is the tag used to create the data types,
 * `_ring` is a pointer to the shared memory (assumed already created), and
 * `_slot_count` is the number of elements that can fit in the ring buffer */
#define RB_INIT(_tag, _ring, _slot_count) do {				\
	(_ring)->slot_count = (_slot_count);				\
	sem_init(&((_ring)->empty), 1, (_ring)->slot_count);		\
	sem_init(&((_ring)->full), 1, 0);				\
	sem_init(&((_ring)->mtx), 1, 1);				\
	(_ring)->client_index = 0;					\
									\
	pthread_mutexattr_t m_attr;					\
	pthread_mutexattr_init(&m_attr);				\
	pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);	\
	pthread_condattr_t c_attr;					\
	pthread_condattr_init(&c_attr);					\
	pthread_condattr_setpshared(&c_attr, PTHREAD_PROCESS_SHARED);	\
	struct _tag##_sring_slot *slot;					\
	for (int i = 0; i < (_ring)->slot_count; ++i) {			\
		slot = &(_ring)->ring[i];				\
		pthread_mutex_init(&slot->mutex, &m_attr);		\
		pthread_cond_init(&slot->condvar, &c_attr);		\
		slot->done = 0;						\
	}								\
} while (0)

/* For the client, makes a request and blocks, waiting for a response. The
 * response is copied to rsp_ptr */
#define RB_MAKE_REQUEST(_tag, _ring, _req_ptr, _rsp_ptr) do {		\
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

/* Server, infinite loop */
#define RB_SERVE(_tag, _ring, _handler) do {				\
	struct _tag##_sring_slot *slot;					\
	int server_index = 0;						\
	while (!done) {							\
		if (sem_wait(&(_ring)->full) == -1) {			\
			int en = errno;					\
			if (en == EINTR)				\
				continue;				\
		}							\
									\
		slot = &(_ring)->ring[server_index];			\
		(_handler)(slot, &reg_handle_request);			\
									\
		sem_post(&(_ring)->empty);				\
		server_index = (server_index + 1) % (_ring)->slot_count;\
	}								\
} while (0)

#endif /* end of include guard: RING_H_ */
