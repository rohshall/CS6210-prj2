/*
 * Macros to handle the definition and flow control of shared synchronous ring
 * buffers.  These macros allow for the creation of ring buffers of arbitrary
 * request and response types, with user-defined request handling.
 *
 * Note that the line continuation markers assume an 8-space tab width.
 */

#ifndef RING_H_
#define RING_H_

/* Defines the types for the ring buffer. Takes in a `_tag` that will be used in
 * defining the type and also in the request/response macros below. Also take
 * in the type of the request, `__req_t`; the type of the response, `__rsp_t`;
 * and the number of slots to create in the ring buffer.
 *
 * Produces three main types:
 * 	union <tag>_sring_entry
 * 		This union has two fields, req and rsp, which are used for
 * 		passing request and response information over the shared buffer.
 * 		The client copies its requst into `req` before notifying the
 * 		server, and the server copies its repsonse into `rsp` before
 * 		notifying the client.
 *
 * 	struct <tag>_sring_slot
 * 		Contains a <tag>_sring_entry and some additional fields for flow
 * 		control, synchronization, etc. Users should not need to use this
 * 		type.
 *
 * 	struct <tag>_sring
 * 		This is the actual shared buffer. It contains some flow control
 * 		and synchronization fields, and an array of <tag>_sring_slot's
 * 		called `ring`. It is this type that should be passed into some
 * 		of the macros below, and it is the size of this type that should
 * 		be passed into shm_open(), and it is to pointers of this type that
 * 		the return of shm_open() should be cast to.
 *
 */
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
	sem_init(&(_ring)->empty, 1, (_ring)->slot_count);		\
	sem_init(&(_ring)->full, 1, 0);					\
	sem_init(&(_ring)->mtx, 1, 1);					\
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

/* For the client, makes a request and then blocks, waiting for a response.
 * `_req_ptr` should be the address of the request, and is first copied into the
 * shared buffer. The server's response is copied to `rsp_ptr` when the server
 * completes its response */
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
	sem_post(&(_ring)->mtx);					\
	sem_post(&(_ring)->full);					\
									\
	pthread_mutex_lock(&slot->mutex);				\
	while(!slot->done)						\
		pthread_cond_wait(&slot->condvar, &slot->mutex);	\
	*(_rsp_ptr) = slot->entry.rsp;					\
	slot->done = 0;							\
	pthread_cond_signal(&slot->condvar);				\
	pthread_mutex_unlock(&slot->mutex);				\
} while (0)

/* Server infinite loop.
 * 	`_tag` is the tag used to create the data structures
 * 	`_ring` is a pointer to a shared memory ring (already created and
 * 		initialized)
 * 	`_stop_cond` is an expression which, when it evaluates to True, breaks
 * 		the loop
 * 	`_handler` is a pointer to a function that takes in a pointer to a union
 * 		sring_entry and arbitrary data in _handler_arg, reads the
 * 		request in the union, and returns its response in the same union
 * 	`_handler_arg` is arbitrary data passed through to `_handler()`
 */
#define RB_SERVE(_tag, _ring, _stop_cond, _handler, _handler_arg) do {	\
	struct _tag##_sring_slot *slot;					\
	int server_index = 0;						\
	while (!(_stop_cond)) {						\
		if (sem_wait(&(_ring)->full) == -1) {			\
			int en = errno;					\
			if (en == EINTR)				\
				continue;				\
		}							\
									\
		slot = &(_ring)->ring[server_index];			\
		pthread_mutex_lock(&slot->mutex);			\
		(_handler)(&slot->entry, (_handler_arg));		\
		slot->done = 1;						\
		pthread_cond_signal(&slot->condvar);			\
		while (slot->done)					\
			pthread_cond_wait(&slot->condvar, &slot->mutex);\
		pthread_mutex_unlock(&slot->mutex);			\
									\
		sem_post(&(_ring)->empty);				\
		server_index = (server_index + 1) % (_ring)->slot_count;\
	}								\
} while (0)

#endif /* end of include guard: RING_H_ */
