#include "namecache.h"
#include <stdlib.h>
#include <string.h>

void tail_dequeue(namecache* cache);

namecache* namecache_init(int new_nodesmax) {
    namecache* new_cache = (namecache*) calloc(1, sizeof(namecache));

    if (new_cache == NULL) {
        return NULL;
    }

    new_cache->head = NULL;
    new_cache->tail = NULL;
    new_cache->nodecount = 0;
    new_cache->nodes_max = new_nodesmax;
    return new_cache;
}

namecache_node* node_init(char* name) {
    namecache_node* new_node = (namecache_node*) calloc(1, sizeof(namecache_node));

    if (new_node == NULL) {
        return NULL;
    }

    new_node->name_data = strndup(name, strlen(name));
    new_node->prev = NULL;
    new_node->next = NULL;

    return new_node;
}

void node_destroy(namecache_node* node) {
    if (node->prev) {
        node->prev->next = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    free(node->name_data);
    node->prev = NULL;
    node->next = NULL;
    free(node);
    node = NULL;
}

void namecache_destroy(namecache* cache) {
    namecache_node* to_destroy = cache->head;

    do {
        namecache_node* next_ref = to_destroy->next;
        node_destroy(to_destroy);
        to_destroy = next_ref;
    } while (to_destroy != NULL);

    cache->head = NULL;
    cache->tail = NULL;
    free(cache);
}

int search_cache(namecache* cache, char* searched_name) {
    namecache_node* current_node = cache->head;

    if (cache->head == NULL || cache->tail == NULL) {
        return 0;
    }

    int nodes_searched = 0;

    do {
        if (strncmp(current_node->name_data, searched_name, strlen(searched_name)) == 0) {
            return 1;
        }

        nodes_searched += 1;

        if (nodes_searched > cache->nodecount) {
            break;
        }

        current_node = current_node->next;
    } while (current_node != cache->tail);

    return 0;
}

int node_enqueue(namecache* cache, char* new_name) {

    if (cache->nodecount == cache->nodes_max) {
        tail_dequeue(cache);
    }

    namecache_node* corresponding_node = node_init(new_name);

    if (corresponding_node == NULL) {
        return -1;
    }

    if (cache->head) {
        cache->head->prev = corresponding_node;
    }

    corresponding_node->next = cache->head;
    cache->head = corresponding_node;

    if (cache->tail == NULL) {
        cache->tail = corresponding_node;
    }

    cache->nodecount += 1;

    return 0;
}

void tail_dequeue(namecache* cache) {
    namecache_node* dequeued_node = cache->tail;
    cache->tail = dequeued_node->prev;
    node_destroy(dequeued_node);
    cache->nodecount -= 1;
}

void node_pluck(namecache* cache, char* name) {
    namecache_node* to_pluck = cache->head;

    do {
        if (strncmp(to_pluck->name_data, name, strlen(name)) == 0) {
            break;
        }
        to_pluck = to_pluck->next;
    } while (to_pluck != cache->tail);

    to_pluck->next->prev = to_pluck->prev;
    to_pluck->prev->next = to_pluck->next;

    to_pluck->prev = NULL;
    to_pluck->next = cache->head;
    cache->head = to_pluck;
}


