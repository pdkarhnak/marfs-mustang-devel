#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

#ifndef __PTHREAD_VECTOR_H__
#define __PTHREAD_VECTOR_H__

/**
 * A dynamic array to store pthread_ts which may be shared and safely 
 * interacted with from an arbitrary number of threads.
 *
 * Meant to be initialized and destroyed in a "parent" (main thread or 
 * similar), but may have its contents checked by pthread_vector_get and its 
 * underlying array contents appended to by pthread_vector_appendset in any 
 * thread.
 */
typedef struct pthread_vector_struct {
    size_t size;
    size_t capacity;
    pthread_t* pthread_id_array;
    pthread_mutex_t* array_lock;
} pthread_vector;

/**
 * Create and initialize a new vector on the heap according to capacity 
 * `new_capacity`. Includes creating and initializing the array of pthread_ts
 * within the vector and the vector's pthread_vector.
 *
 * Returns: valid heap pointer to a pthread_vector on success, or NULL on 
 * failure with errno set to ENOMEM from wrapped calloc() calls.
 */
pthread_vector* pthread_vector_init(size_t new_capacity);

/**
 * Safely get a pthread_t element from a the given vector's underlying 
 * pthread_t array at the specified `index`.
 *
 * Returns: valid pthread_t from the vector's array on success, or 0 on failure
 * with errno set (EINVAL for bad index, NULL vector, or vector array 
 * nonexistent).
 */
pthread_t pthread_vector_get(pthread_vector* vector, size_t index);

/**
 * Safely append one or more pthread_ts (up to `count` pthread_ts) from the 
 * buffer `thread_ids` to a vector's underlying array of pthread_ts.
 *
 * NOTE: this function "silently" performs size and capacity checks, resizing
 * the vector's underlying array as necessary.
 *
 * Returns: 0 on success (all pthread_ts in thread_ids[0..count] added), or -1
 * on failure with errno set (EINVAL for vector NULL or vector array NULL, or
 * ENOMEM if resizing the vector's underlying array to fit new data failed).
 */
int pthread_vector_appendset(pthread_vector* vector, pthread_t* thread_ids, size_t count);

/**
 * "Poll" (join and get return value of) a particular thread whose pthread_t ID
 * is stored within a vector's underlying array at `index`.
 *
 * Returns: return value of pthread_join (0 on success/errno on failure), or -1
 * if wrapped pthread_vector_get() call failed at that index, with errno 
 * "forwarded" appropriately (see pthread_vector_get() header).
 */
int pthread_vector_pollthread(pthread_vector* vector, void** retval_ptr, size_t index);

/**
 * Safely destroy a pthread_vector, including the vector's underlying pthread_t
 * array and associated mutex.
 *
 * NOTE: this function accommodates synchronization problems with checking 
 * mutex use by freeing the vector's underlying array and setting it to NULL
 * atomically. Other threads will then see that the array is NULL and release
 * the mutex.
 *
 * Synchronization aside, this function should only be called in a single 
 * "manager" or "parent" thread once all threads in the vector's underlying 
 * array have been verified to exit through calls to 
 * pthread_vector_pollthread() for all applicable threads.
 */
void pthread_vector_destroy(pthread_vector* vector);

#endif
