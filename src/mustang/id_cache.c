#include "id_cache.h"
#include <string.h>
#include <errno.h>

/* Private functions */
id_cachenode* cachenode_init(char* new_id) {
    id_cachenode* new_cachenode = (id_cachenode*) calloc(1, sizeof(id_cachenode));

    if (new_cachenode == NULL) {
        return NULL;
    }

    new_cachenode->prev = NULL;
    new_cachenode->next = NULL;

    new_cachenode->id = strdup(new_id);

    if (new_cachenode->id == NULL) {
        free(new_cachenode);
        return NULL;
    }

    return new_cachenode;
}

void cachenode_destroy(id_cachenode* to_destroy) {
    if (to_destroy == NULL) {
        return;
    }

    free(to_destroy->id);

    if (to_destroy->prev != NULL) {
        to_destroy->prev->next = to_destroy->next;
    }

    if (to_destroy->next != NULL) {
        to_destroy->next->prev = to_destroy->prev;
    }

    to_destroy->next = NULL;
    to_destroy->prev = NULL;
    free(to_destroy);
}

id_cache* id_cache_init(size_t new_capacity) {
    id_cache* new_cache = calloc(1, sizeof(id_cache));

    if (new_cache == NULL) {
        return NULL;
    }

    new_cache->head = NULL;
    new_cache->tail = NULL;
    new_cache->size = 0;
    new_cache->capacity = new_capacity;

    return new_cache;
}

void update_tail(id_cache* cache) {
    if (cache == NULL) {
        return;
    }

    id_cachenode* current_node = cache->head;

    while (current_node != NULL) {
        if (current_node->next == NULL) {
            cache->tail = current_node;
            break;
        }

        current_node = current_node->next;
    }
}

int id_cache_probe(id_cache* cache, char* searched_id) {
    if ((cache == NULL) || (searched_id == NULL)) {
        return 0;
    }

    if (cache->size == 0) {
        return 0;
    }

    id_cachenode* searched_node = cache->head;

    do {
        if (strncmp(searched_node->id, searched_id, strlen(searched_node->id)) == 0) {
            if (searched_node != cache->head) {
                if (searched_node->next != NULL) {
                    searched_node->next->prev = searched_node->prev;
                } 
                
                if (searched_node->prev != NULL) {
                    searched_node->prev->next = searched_node->next;
                }

                searched_node->prev = NULL;
                searched_node->next = cache->head;

                cache->head = searched_node;
                update_tail(cache);
            }

            return 1;
        }

        searched_node = searched_node->next;
    } while (searched_node != NULL);

    return 0;
}

int id_cache_add(id_cache* cache, char* new_id) {
    id_cachenode* added_node = cachenode_init(new_id);

    if (added_node == NULL) {
        return -1;
    }

    added_node->next = cache->head;

    if (cache->head != NULL) {
        cache->head->prev = added_node;
    }

    cache->head = added_node;

    // Unconditionally increment size, then clean up to enforce capacity as 
    // needed.
    cache->size += 1; 

    // If the cache was empty at the time of this addition, also mark the new 
    // node as the cache's tail node.
    update_tail(cache);


    if (cache->size > cache->capacity) {
        cachenode_destroy(cache->tail);
        cache->size -= 1;
    }

    return 0;
}

void id_cache_destroy(id_cache* cache) {
    if (cache == NULL) {
        return;
    }

    id_cachenode* destroyed_node = cache->head;

    do {
        id_cachenode* next_ref = destroyed_node->next;
        cachenode_destroy(destroyed_node);
        destroyed_node = next_ref;
    } while (destroyed_node != NULL);

    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
    free(cache);
}
