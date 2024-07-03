/*
Copyright (c) 2015, Los Alamos National Security, LLC
All rights reserved.

Copyright 2015.  Los Alamos National Security, LLC. This software was
produced under U.S. Government contract DE-AC52-06NA25396 for Los
Alamos National Laboratory (LANL), which is operated by Los Alamos
National Security, LLC for the U.S. Department of Energy. The
U.S. Government has rights to use, reproduce, and distribute this
software.  NEITHER THE GOVERNMENT NOR LOS ALAMOS NATIONAL SECURITY,
LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LIABILITY
FOR THE USE OF THIS SOFTWARE.  If software is modified to produce
derivative works, such modified software should be clearly marked, so
as not to confuse it with the version available from LANL.
 
Additionally, redistribution and use in source and binary forms, with
or without modification, are permitted provided that the following
conditions are met: 1. Redistributions of source code must retain the
above copyright notice, this list of conditions and the following
disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. Neither the name of Los Alamos National Security, LLC, Los Alamos
National Laboratory, LANL, the U.S. Government, nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS
ALAMOS NATIONAL SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-----
NOTE:
-----
MarFS is released under the BSD license.

MarFS was reviewed and released by LANL under Los Alamos Computer Code
identifier: LA-CC-15-039.

MarFS uses libaws4c for Amazon S3 object communication. The original version
is at https://aws.amazon.com/code/Amazon-S3/2601 and under the LGPL license.
LANL added functionality to the original work. The original work plus
LANL contributions is found at https://github.com/jti-lanl/aws4c.

GNU licenses can be found at http://www.gnu.org/licenses/.
*/

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdint.h>
#include <stdio.h>

#define KEY_SEED 43 // a prime number arbitrarily and pseudorandomly chosen from the range [13, 173] to seed the hashing algorithm

typedef struct hashtable_struct {  
    size_t capacity;
    size_t capacity_mask;
    char** stored_nodes;
} hashtable;

/**
 * Initialize a hashtable on the heap, including space for all `new_capacity`
 * hashnodes.
 */
hashtable* hashtable_init(size_t new_capacity);

/**
 * Destroy a hashtable on the heap, freeing all memory associated with the
 * table including the space for all CAPACITY nodes.
 */
int hashtable_destroy(hashtable* table);

/** 
 * The public function to get an associated value for a particular key.
 * 
 * Given a name (intended alphanumeric object name string), return the stored
 * copy of the name if stored in the table.
 *
 * Returns: associated copy of key from table if key represented in given
 * table, sentinel NULL otherwise.
 */
char* get(hashtable* table, char* name_key);

/** 
 * The public function to insert an object name into a particular hash table.
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

/**
 * A helper function to print the contents of non-NULL hashnodes in a hashtable
 * to the file referenced by the pointer `output`.
 *
 * NOTE: this function assumes any synchronization measures (locking an 
 * associated mutex for hashtable `table`, etc.) have already been taken. In 
 * mustang, the engine (main routine) locks and unlocks the associated mutex 
 * around a call to this function to satisfy the synchronization assumptions.
 * Do not otherwise call this function without appropriately synchronizing on 
 * the hashtable.
 */
void hashtable_dump(hashtable* table, FILE* output);

#endif
