#include "namecache.h"
#include <stdlib.h>
#include <string.h>

nc_node* node_init(char* new_data) {
    nc_node* new_node = (nc_node*) calloc(1, sizeof(nc_node));
    
    if (new_node == NULL) {
        return NULL;
    }

    new_node->data = strndup(new_data, strlen(new_data));
    new_node->next = NULL;

    return new_node;
}

void node_destroy(nc_node* node) {
    if (node == NULL) {
        return;
    }

    free(node->data);
    node->next = NULL;
    free(node);
}

namecache* namecache_init(int new_capacity) {
    namecache* new_cache = (namecache*) calloc(1, sizeof(namecache));

    if (new_cache == NULL) {
        return NULL;
    }

    new_cache->head = NULL;
    new_cache->tail = NULL;
    new_cache->size = 0;

    new_cache->capacity = (new_capacity > 0) ? new_capacity : NAMECACHE_CAP_DEFAULT;

    return new_cache;
}

void namecache_destroy(namecache* cache) {
    if (cache == NULL) {
        return;
    }    

    nc_node* to_destroy = cache->head;

    if (to_destroy == NULL) {
        return;
    }

    do {
        nc_node* next_ref = to_destroy->next;
        node_destroy(to_destroy);
        to_destroy = next_ref;
    } while (to_destroy != NULL);

    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;

    free(cache);
}

int search_cache(namecache* cache, char* searched_data) {
    if (cache == NULL) {
        return -1;
    }

    if (cache->size == 0) {
        return 0;
    }

    nc_node* searched_node = cache->head;

    do {
        if (strncmp(searched_node->data, searched_data, strlen(searched_data)) == 0) {
            return 1;
        }
        searched_node = searched_node->next;
    } while (searched_node != NULL);

    return 0;
}

int namecache_enqueue(namecache* cache, char* new_data) {
    if (search_cache(cache, new_data)) {
        return 1;
    }

    nc_node* new_node = node_init(new_data);
    
    if (new_node == NULL) {
        return -1;
    }

    if (cache->size >= cache->capacity) {
        nc_node* new_tail = cache->head;

        while (new_tail->next != cache->tail) {
            new_tail = new_tail->next;
        }

        node_destroy(cache->tail);
        cache->tail = new_tail;
        cache->size -= 1;
    }

    if (cache->size == 0) {
        cache->tail = new_node;
    }

    new_node->next = cache->head;
    cache->head = new_node;
    cache->size += 1;

    return 0;
}

void namecache_pluck(namecache* cache, char* keyword) {
    if (!search_cache(cache, keyword)) {
        return;
    }

    if (strncmp(cache->head->data, keyword, strlen(keyword)) == 0) {
        return;
    }

    nc_node* search_base = cache->head;

    while (search_base->next != NULL) {
        nc_node* to_compare = search_base->next;

        if (strncmp(to_compare->data, keyword, strlen(keyword)) == 0) {
            search_base->next = to_compare->next;
            to_compare->next = cache->head;
            cache->head = to_compare;
            return;
        }

        search_base = to_compare;
    }
}
