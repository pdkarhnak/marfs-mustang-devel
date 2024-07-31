#ifndef __MUSTANG_TASK_QUEUE_H__
#define __MUSTANG_TASK_QUEUE_H__

#include <stdlib.h>
#include <pthread.h>
#include <config/config.h>
#include "hashtable.h"

typedef struct mustang_task_struct mustang_task;

typedef struct mustang_task_queue_struct task_queue;

typedef struct mustang_task_struct {
    marfs_config* config;
    marfs_position* position;
    hashtable* ht;
    pthread_mutex_t* ht_lock;
    task_queue* queue_ptr; // Tasks are retrieved from the queue, but during task execution other tasks may need to be enqueued.
    // The routine to execute. For the current version of Mustang (1.2.0), either `traverse_ns()` for a namespace or `traverse_dir()` for a regular directory.
    // If NULL, workers will detect this, clean up their state, and exit.
    void (*task_func)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*, task_queue*);
    mustang_task* prev; // Queue implemented as doubly-linked list of tasks
    mustang_task* next;
} mustang_task;

typedef struct mustang_task_queue_struct {
    size_t size;
    size_t capacity; // The maximum number of tasks that may be enqueued without causing a meaningful wait on the space_available cv.
    size_t todos; // Number of tasks that are currently either in the queue or in progress. Used to synchronize manager with workers (together with manager_cv).
    mustang_task* head; // Head and tail pointers maintained for constant-time enqueue and dequeue
    mustang_task* tail;
    pthread_mutex_t* lock; // Makes any interactions with queue state (tasks in queue, size, etc.) atomic.
    pthread_cond_t* task_available; // Worker threads wait on this cv until at least one task is available to dequeue.
    pthread_cond_t* space_available;
    pthread_cond_t* manager_cv; // The synchronization point between manager and workers to indicate whether all work is finished.
} task_queue;

/**
 * Allocate space for, and return a pointer to, a new mustang_task struct on 
 * the heap. Initialize the task with all necessary state (MarFS config, MarFS
 * position, hashtable ref, hashtable lock, task queue ref, and function 
 * pointer indicating what routine to execute).
 *
 * Returns: valid pointer to mustang_task struct on success, or NULL on 
 * failure.
 */
mustang_task* task_init(marfs_config* task_config, marfs_position* task_position, hashtable* task_ht, pthread_mutex_t* task_ht_lock, task_queue* task_queue_ref, void (*traversal_routine)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*, task_queue*));

/**
 * Allocate space for, and return a pointer to, a new task_queue struct on the 
 * heap according to a specified capacity.
 *
 * Returns: valid pointer to task_queue struct on success, or NULL on failure.
 *
 * NOTE: this function may return NULL under any of the following conditions:
 * - Failure to calloc() queue space 
 * - Failure to calloc() queue mutex
 * - pthread_mutex_init() failure for queue mutex
 * - Failure to calloc() space for at least one queue condition variable
 * - pthread_cond_init() failure for at least one queue condition variable
 */
task_queue* task_queue_init(size_t new_capacity);

/**
 * Atomically enqueue a new task `new_task` to the given task queue `queue`, 
 * adjusting internal queue state as necessary to reflect changes (new size, 
 * new head/tail nodes in queue, etc.).
 *
 * Returns: 0 on success, or -1 on failure with errno set to EINVAL (queue
 * is NULL).
 *
 * NOTE: this function wraps a pthread_cond_wait() loop on the queue's 
 * `space_available` cv field, so callers do not need to (and, for application
 * efficiency/to minimize lock contention, should not) separately lock the 
 * queue's lock and wait on the `space_available` cv.
 */
int task_enqueue(task_queue* queue, mustang_task* new_task);

/**
 * Atomically dequeue (unlink) and return the `mustang_task` struct at the head
 * of the task queue `queue`, adjusting internal queue state as necessary to 
 * reflect changes (new size, head/tail nodes, etc.).
 *
 * Returns: valid pointer to mustang_task struct on success, or NULL on failure
 * with errno set (EINVAL for queue == NULL).
 *
 * NOTE: in a similar fashion to task_enqueue, this function wraps a 
 * pthread_cond_wait() loop on a queue condition variable (the task_available 
 * cv in this case) before returning. Callers do not need to separately wait, 
 * and, to keep lock contention to a minimum, should not.
 */
mustang_task* task_dequeue(task_queue* queue);

/**
 * Destroy the given task_queue struct and free the memory associated with it.
 */
int task_queue_destroy(task_queue* queue);

#endif
