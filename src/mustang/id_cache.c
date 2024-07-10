#include "id_cache.h"
#include <string.h>
#include <errno.h>

// Prototypes for private functions
id_cachenode* cachenode_init(char* new_id);
void update_tail(id_cache* cache);
void pluck_node(id_cache* cache, id_cachenode* node);
void cachenode_destroy(id_cachenode* node);

// Public interface implementation
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

int id_cache_add(id_cache* cache, char* new_id) {
    id_cachenode* new_node = cachenode_init(new_id);

    if (new_node == NULL) {
        return -1;
    }

    return -1;
}

int id_cache_probe(id_cache* cache, char* searched_id) {
    return 0;
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

