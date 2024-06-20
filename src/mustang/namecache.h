#ifndef __NAMECACHE_H__
#define __NAMECACHE_H__

typedef struct namecache_node_struct namecache_node;

typedef struct namecache_struct namecache;

typedef struct namecache_node_struct {
    char* name_data;
    namecache_node* prev;
    namecache_node* next;
} namecache_node;

typedef struct namecache_struct {
    namecache_node* head;
    namecache_node* tail;
    int nodecount;
    int nodes_max;
} namecache;

namecache* namecache_init(int new_nodesmax);

void namecache_destroy(namecache* cache);

int search_cache(namecache* cache, char* searched_name);

int node_enqueue(namecache* cache, char* new_name);

void node_pluck(namecache* cache, char* name);

#endif
