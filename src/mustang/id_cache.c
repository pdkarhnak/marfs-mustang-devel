#include "id_cache.h"
#include <string.h>
#include <errno.h>

/**** Prototypes for private functions ****/
id_cachenode* cachenode_init(char* new_id);
void update_tail(id_cache* cache);
void cachenode_destroy(id_cachenode* node);
void pluck_node(id_cache* cache, id_cachenode* node);

/**** Public interface implementation ****/

/**
 * Allocate space for, and return a pointer to, a new id_cache struct on the 
 * heap according to a specified capacity.
 *
 * Returns: valid pointer to id_cache struct on success, or NULL on failure.
 */
id_cache* id_cache_init(size_t new_capacity) {
    id_cache* new_cache = (id_cache*) calloc(1, sizeof(id_cache));

    if (new_cache == NULL) {
        return NULL;
    }

    new_cache->capacity = new_capacity;
    new_cache->size = 0;
    new_cache->head = NULL;
    new_cache->tail = NULL;

    return new_cache;
}

/** 
 * Create a new cache node to store `new_id`, then place that node at the head
 * of the cache to indicate that the `new_id` is the most-recently-used ID in
 * the cache. If the cache is at capacity, silently evict the tail node in the
 * cache to make room for the new node.
 *
 * Returns: 0 on success (node could be created and the list was successfully
 * modified), or -1 on failure (node could not be allocated).
 */
int id_cache_add(id_cache* cache, char* new_id) {
    id_cachenode* new_node = cachenode_init(new_id);

    if (new_node == NULL) {
        return -1;
    }

    if (cache->size == 0) {
        cache->head = new_node;
    } else {
        new_node->next = cache->head;
        cache->head->prev = new_node;
        cache->head = new_node;
    }

    cache->size += 1;
    update_tail(cache);

    if (cache->size > cache->capacity) {
        cachenode_destroy(cache->tail);
        cache->size -= 1;
        update_tail(cache);
    }

    return 0;
}

/** 
 * Check an id_cache struct for a node which stores ID `searched_id`. If the ID
 * is stored, silently "bump" the node storing the searched ID to the head of 
 * the cache to indicate that the ID has been the most recently used in the
 * cache.
 *
 * Returns: 1 if a node was found which stored an id matching `stored_id`, or 0
 * if no such node was present.
 */
int id_cache_probe(id_cache* cache, char* searched_id) {
    id_cachenode* searched_node = cache->head;

    while (searched_node != NULL) {
        if (strncmp(searched_node->id, searched_id, strlen(searched_node->id)) == 0) {
            pluck_node(cache, searched_node);
            update_tail(cache);
            return 1;
        }

        searched_node = searched_node->next;
    }

    return 0;
}

/**
 * Destroy the given id_cache struct and free the memory associated with it.
 */
void id_cache_destroy(id_cache* cache) {
    if (cache == NULL) {
        return;
    }

    id_cachenode* to_destroy = cache->head;

    while (to_destroy != NULL) {
        id_cachenode* next_node = to_destroy->next;
        cachenode_destroy(to_destroy);
        to_destroy = next_node;
    }

    cache->size = 0;
    cache->head = NULL;
    cache->tail = NULL;
    free(cache);
}

/**** Private functions ****/
id_cachenode* cachenode_init(char* new_id) {
    id_cachenode* new_node = calloc(1, sizeof(id_cachenode));

    if (new_node == NULL) {
        return NULL;
    }
    
    char* duped_id = strdup(new_id);
    if (duped_id == NULL) {
        free(new_node);
        return NULL;
    }

    new_node->id = duped_id;
    new_node->prev = NULL;
    new_node->next = NULL;

    return new_node;
}

void update_tail(id_cache* cache) {
    if (cache == NULL) {
        return;
    }

    id_cachenode* current_node = cache->head;

    while (current_node != NULL) {
        if (current_node->next == NULL) {
            cache->tail = current_node;
        }

        current_node = current_node->next;
    }
}

void cachenode_destroy(id_cachenode* node) {
    if (node == NULL) {
        return;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    }

    node->prev = NULL;
    node->next = NULL;
    free(node->id);
    free(node);
}

void pluck_node(id_cache* cache, id_cachenode* node) {
    if (cache == NULL || node == NULL) {
        return;
    }

    if (cache->head == node) {
        return;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    node->prev = NULL;
    node->next = cache->head;
    cache->head->prev = node;
    cache->head = node;
}
