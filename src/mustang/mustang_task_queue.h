#ifndef __MUSTANG_TASK_QUEUE_H__
#define __MUSTANG_TASK_QUEUE_H__

#include <stdlib.h>
#include <pthread.h>
#include <config/config.h>
#include "hashtable.h"

typedef struct mustang_task_struct mustang_task;

typedef struct mustang_task_struct {
    marfs_config* config;
    marfs_position* position;
    hashtable* ht;
    pthread_mutex_t* ht_lock;
    void (*traversal_func)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*);
    mustang_task* prev;
    mustang_task* next;
} mustang_task;

typedef struct mustang_task_queue_struct task_queue;

typedef struct task_queue_struct {
    size_t size;
    size_t capacity;
    size_t todos;
    mustang_task* head;
    mustang_task* tail;
    pthread_mutex_t* lock;
    pthread_cond_t* task_available;
    pthread_cond_t* space_available;
    pthread_cond_t* manager_cv;
} task_queue;

mustang_task* task_init(marfs_config* task_config, marfs_position* task_position, hashtable* task_ht, pthread_mutex_t* task_ht_lock, void (*traversal_routine)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*));

/**
 * Allocate space for, and return a pointer to, a new task_queue struct on the 
 * heap according to a specified capacity.
 *
 * Returns: valid pointer to task_queue struct on success, or NULL on failure.
 */
task_queue* task_queue_init(size_t new_capacity);

/** 
 * Create a new cache node to store `new_id`, then place that node at the head
 * of the cache to indicate that the `new_id` is the most-recently-used ID in
 * the cache. If the cache is at capacity, silently evict the tail node in the
 * cache to make room for the new node.
 *
 * Returns: 0 on success (node could be created and the list was successfully
 * modified), or -1 on failure (node could not be allocated).
 */
int task_enqueue(task_queue* queue, mustang_task* new_task);

/** 
 *
 * Check an task_queue struct for a node which stores ID `searched_id`. If the ID
 * is stored, silently "bump" the node storing the searched ID to the head of 
 * the cache to indicate that the ID has been the most recently used in the
 * cache.
 *
 * Returns: 1 if a node was found which stored an id matching `stored_id`, or 0
 * if no such node was present.
 */
mustang_task* task_dequeue(task_queue* queue);

/**
 * Destroy the given task_queue struct and free the memory associated with it.
 */
int task_queue_destroy(task_queue* queue);

#endif
