#ifndef __ID_CACHE_H__
#define __ID_CACHE_H__

#include <stdlib.h>

typedef struct id_cachenode_struct id_cachenode;

typedef struct id_cachenode_struct {
    char* id;
    id_cachenode* prev;
    id_cachenode* next;
} id_cachenode;

typedef struct id_cache_struct id_cache;

typedef struct id_cache_struct {
    size_t size;
    size_t capacity;
    id_cachenode* head;
    id_cachenode* tail;
} id_cache;

/**
 * Allocate space for, and return a pointer to, a new id_cache struct on the 
 * heap according to a specified capacity.
 *
 * Returns: valid pointer to id_cache struct on success, or NULL on failure.
 */
id_cache* id_cache_init(size_t new_capacity);

/** 
 * Create a new cache node to store `new_id`, then place that node at the head
 * of the cache to indicate that the `new_id` is the most-recently-used ID in
 * the cache. If the cache is at capacity, silently evict the tail node in the
 * cache to make room for the new node.
 *
 * Returns: 0 on success (node could be created and the list was successfully
 * modified), or -1 on failure (node could not be allocated).
 */
int id_cache_add(id_cache* cache, char* new_id);

/** 
 *
 * Check an id_cache struct for a node which stores ID `searched_id`. If the ID
 * is stored, silently "bump" the node storing the searched ID to the head of 
 * the cache to indicate that the ID has been the most recently used in the
 * cache.
 *
 * Returns: 1 if a node was found which stored an id matching `stored_id`, or 0
 * if no such node was present.
 */
int id_cache_probe(id_cache* cache, char* searched_id);

/**
 * Destroy the given id_cache struct and free the memory associated with it.
 */
void id_cache_destroy(id_cache* cache);

#endif
