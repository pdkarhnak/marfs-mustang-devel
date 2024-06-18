#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdint.h>

/**
 * A customizable constant which will directly control the maximum number of
 * hash nodes and indirectly control the general likelihood of hash collisions.
 */
#ifndef CAPACITY
#define CAPACITY (1024)
#define CAPACITY_MASK ((uint64_t) (CAPACITY - 1))    // assuming CAPACITY is aligned to a power of 2 for "clean" bitmasking
#endif

#define KEY_SEED 43     // A prime number arbitrarily (and pseudorandomly) selected from the range [13, 173]

// Essentially a "direct" alias for an array of char* elements
typedef struct hashtable_struct {  
    char* stored_nodes[CAPACITY];
} hashtable;

/**
 * Initialize a hashtable on the heap, including space for all CAPACITY
 * hashnodes.
 */
hashtable* hashtable_init(void);

/**
 * Destroy a hashtable on the heap, freeing all memory associated with the
 * table including the space for all CAPACITY nodes.
 */
int hashtable_destroy(hashtable* table);

/** 
 * The public method to get an associated value for a particular key.
 * 
 * Given a name (intended alphanumeric object name string), return the stored
 * copy of the name if stored in the table.
 *
 * Returns: associated copy of key from table if key represented in given
 * table, sentinel NULL otherwise.
 */
char* get(hashtable* table, char* name_key);

/** 
 * The public method to insert an object name into a particular hash table.
 *
 * For a given name key, compute the hashcode and insert the name at the
 * appropriate index within the hashtable.
 *
 * NOTE: in a case of application-specific behavior, this function always
 * succeeds (and thus does not return anything). The function will either
 * insert the object name into the table if it is not present at the computed
 * index or will simply return without inserting upon encountering a duplicate.
 */
void put(hashtable* table, char* new_object_name);

void hashtable_dump(hashtable* table);

#endif
