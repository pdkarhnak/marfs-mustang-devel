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

#ifndef __RETCODE_LL_H__
#define __RETCODE_LL_H__

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>

#define RC_LL_LEN_MAX 128

/** 
 * A collection of retcode settings from which a bitwise OR of one or more
 * will produce a thread's return value in `thread_main`.
 */
typedef enum {
    RETCODE_SUCCESS =           0x0,
    ALLOC_FAILED,            /* 0x1 */
    DIR_OPEN_FAILED,         /* 0x2 */
    NEW_DIRFD_OPEN_FAILED =     0x4,
    THREADARG_FORK_FAILED =     0x8,
    PTHREAD_CREATE_FAILED =     0x10,
    PTHREAD_JOIN_FAILED =       0x20,
    CHILD_ALLOC_FAILED =        0x40, /* Child couldn't set up and returned NULL */
    DUPPOS_FAILED =             0x80,
    TRAVERSE_FAILED =           0x100,
    CLOSEDIR_FAILED =           0x200,
    ABANDONPOS_FAILED =         0x400,
    FORTIFYPOS_FAILED =         0x800
} RETCODE_FLAGS;

typedef struct retcode_struct retcode;

/**
 * A doubly-linked list node intended to be contained within a wrapping 
 * retcode_ll struct (which maintains extra information regarding list head 
 * and tail). Nodes are maintained per-thread; since mustang creates one 
 * thread per directory to traverse, nodes represent the results of traversing
 * a single directory (whose location is given by the relative location at 
 * `basepath`) once they are returned from the threads that interact with them.
 *
 * Each node records its own pthread_t ID and will have its flags value 
 * updated as error conditions are encountered. Once placed within a collection
 * that a retcode_ll struct wraps, prev and next contain meaningful references
 * to either other valid nodes or to NULL.
 */
typedef struct retcode_struct {
    pthread_t self;
    RETCODE_FLAGS flags;
    char* basepath;
    retcode* prev;
    retcode* next;
} retcode;

/**
 * A "wrapper" struct for a linked list of retcode nodes (see above) that 
 * tracks additional state information for list size (number of linked nodes),
 * list head node, and list tail node.
 *
 * "Visible" interactions with retcode objects in thread routines occur more
 * with their wrapper retcode_ll structs than with the retcode nodes 
 * themselves.
 */
typedef struct retcode_ll_struct {
    int size;
    retcode* head;
    retcode* tail;
} retcode_ll;

/**
 * Create a new return code struct given a basepath and flags arguments. 
 * This function will take the pthread_t given from pthread_self() of the 
 * calling thread and appropriately record the result in the node struct's 
 * pthread_t member. Since this function creates a single node and does not
 * interact with any relevant collection, next and prev pointers are made NULL.
 *
 * Nodes initialized using this function should be collected into a wrapping 
 * linked list using retcode_ll_add().
 *
 * Returns: valid pointer to new node on success, or NULL on failure.
 */
retcode* node_init(char* new_basepath, RETCODE_FLAGS new_flags);

/**
 * Create and initialize a new linked list of thread return codes, recording
 * both the node collection itself and state information (node count and head
 * and tail pointers for the linked list). This does *not* allocate any space
 * for specific thread returncode nodes ("retcode" nodes), which must instead
 * be allocated using node_init() and added to a linked list using 
 * retcode_ll_add().
 *
 * Returns: valid heap pointer to a retcode_ll on success, or NULL on 
 * failure with errno set to ENOMEM from wrapped calloc() calls.
 */
retcode_ll* retcode_ll_init(void);

/**
 * Inserts a new retcode node at the tail of the underlying linked list 
 * collection within `rll`. Since the retcode_ll struct maintains head and tail
 * pointers for the relevant list, this operation occurs in constant time.
 *
 * Returns: 0 on success (including a valid reference `rll` containing an empty
 * list), or -1 on failure (rll or node NULL) with errno set to EINVAL.
 */
int retcode_ll_add(retcode_ll* rll, retcode* node);

/**
 * Concatenate the underlying list (collection) of retcode_ll `src` with the
 * underlying collection of retcode_ll `dest`, then free the wrapper struct
 * for src. The order this occurs in is by "appending" the list in `src` to 
 * the list in `dest`.
 *
 * In effect, this joins the lists as one within `dest` (a pointer to
 * which is returned) and updates the `dest` list state as needed to reflect 
 * size and tail node changes.
 */
retcode_ll* retcode_ll_concat(retcode_ll* dest, retcode_ll* src);

/**
 * Safely flush the contents of the retcode_ll `list` to the relevant logging 
 * file specified by `logfile` by surrounding write operations with locks and
 * unlocks on `logfile_lock`. Once writing is complete, empty (free) the space
 * associated with the retcode collection of `list`.
 */
void retcode_ll_flush(retcode_ll* rll, FILE* logfile, pthread_mutex_t* logfile_lock);

/** 
 * Destroy a retcode_ll struct and free all associated space, including for
 * each of the contained retcode nodes.
 *
 * NOTE: unlike in retcode_ll_flush, the retcode_ll reference `rll` is no 
 * longer valid once this function returns because both the wrapped list and 
 * the struct itself have been freed.
 */
void retcode_ll_destroy(retcode_ll* rll);

#endif
