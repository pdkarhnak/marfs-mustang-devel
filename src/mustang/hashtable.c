#include "hashtable.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/** 
 * Internal hashing function: MurmurHash3_x64_128 by Austin Appleby. See
 * release notes and full implementation, including helper utilities and
 * macros, below.
 *
 * Duplicated from the MarFS public repository at:
 * https://github.com/mar-file-system/marfs/blob/master/src/hash/hash.c
 */
void MurmurHash3_x64_128( const void* key, const int len, const uint32_t seed,
        void* out );

/**
 * A wrapper around the MurmurHash3 calls to return a "friendly" 
 * capacity-aligned index.
 */
uint64_t hashcode(char* name) {
    uint64_t murmur_result[2];
    MurmurHash3_x64_128(name, strlen(name), KEY_SEED, murmur_result);
    return murmur_result[0] & CAPACITY_MASK;
}

hashtable* hashtable_init(void) {
    hashtable* new_table = calloc(1, sizeof(hashtable));

    // CAPACITY constant defined in "hashtable.h"
    for (int node_index = 0; node_index < CAPACITY; node_index += 1) {
        new_table->stored_nodes[node_index] = NULL;
    }

    return new_table;
}

int hashtable_destroy(hashtable* table) {
    for (int node_index = 0; node_index < CAPACITY; node_index += 1) {
        if (table->stored_nodes[node_index]) {
            free(table->stored_nodes[node_index]);
        }
    }
    
    free(table); 

    return 0;
}

char* get(hashtable* table, char* key) {
    // Recompute the hash to see which index (and, therefore, which relevant
    // node) to search
    uint64_t computed_hashcode = hashcode(key); 
    return table->stored_nodes[computed_hashcode];
}

void put(hashtable* table, char* new_object_name) {
    // Compute the hash to see which index (and, therefore, which relevant
    // node) to insert at
    
    uint64_t mapped_hashcode = hashcode(new_object_name);

    // TODO: consider how to fix memory leaks if hash collision occurs (i.e., how to catch-and-free within this function?)
    if ((table->stored_nodes)[mapped_hashcode] != NULL) {
        return;
    } else {
        (table->stored_nodes)[mapped_hashcode] = strdup(new_object_name);
    }
}

void hashtable_dump(hashtable* table) {
    for (size_t index = 0; index < CAPACITY; index += 1) {
        if ((table->stored_nodes)[index] != NULL) {
            printf("Index %zu:\t%s\n", index, (table->stored_nodes)[index]);
        }
    }
}

/* ----- END HASHTABLE IMPLEMENTATION ----- */

// The following implementation of MurmurHash was retrieved from --
//    https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
// 
// The release info for the code has been reproduced below.
//
//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//


#define BIG_CONSTANT(x) (x##LLU)
#define FORCE_INLINE inline __attribute__((always_inline))
#define ROTL64(x,y) rotl64(x,y)


static FORCE_INLINE uint64_t rotl64 ( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here
#define getblock64(p,i) (p[i])

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche
static FORCE_INLINE uint64_t fmix64 ( uint64_t k )
{
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}


void MurmurHash3_x64_128 ( const void * key, const int len,
                           const uint32_t seed, void * out )
{
  const uint8_t * data = (const uint8_t*)key;
  const int nblocks = len / 16;

  uint64_t h1 = seed;
  uint64_t h2 = seed;

  const uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
  const uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

  //----------
  // body

  const uint64_t * blocks = (const uint64_t *)(data);

  int i = 0;
  for(; i < nblocks; i++)
  {
    uint64_t k1 = getblock64(blocks,i*2+0);
    uint64_t k2 = getblock64(blocks,i*2+1);

    k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;

    h1 = ROTL64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;

    k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

    h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
  }

  //----------
  // tail

  const uint8_t * tail = (const uint8_t*)(data + nblocks*16);

  uint64_t k1 = 0;
  uint64_t k2 = 0;

  switch(len & 15)
  {
  case 15: k2 ^= ((uint64_t)tail[14]) << 48;
  case 14: k2 ^= ((uint64_t)tail[13]) << 40;
  case 13: k2 ^= ((uint64_t)tail[12]) << 32;
  case 12: k2 ^= ((uint64_t)tail[11]) << 24;
  case 11: k2 ^= ((uint64_t)tail[10]) << 16;
  case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;
  case  9: k2 ^= ((uint64_t)tail[ 8]) << 0;
           k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

  case  8: k1 ^= ((uint64_t)tail[ 7]) << 56;
  case  7: k1 ^= ((uint64_t)tail[ 6]) << 48;
  case  6: k1 ^= ((uint64_t)tail[ 5]) << 40;
  case  5: k1 ^= ((uint64_t)tail[ 4]) << 32;
  case  4: k1 ^= ((uint64_t)tail[ 3]) << 24;
  case  3: k1 ^= ((uint64_t)tail[ 2]) << 16;
  case  2: k1 ^= ((uint64_t)tail[ 1]) << 8;
  case  1: k1 ^= ((uint64_t)tail[ 0]) << 0;
           k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len; h2 ^= len;

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  ((uint64_t*)out)[0] = h1;
  ((uint64_t*)out)[1] = h2;
}


