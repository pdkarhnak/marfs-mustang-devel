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

#ifndef __PTHREAD_VECTOR_H__
#define __PTHREAD_VECTOR_H__

#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include "mustang_threading.h"

typedef struct retcode_struct retcode;

typedef struct retcode_struct {
    uint32_t flags;
    pthread_t self;
    char* basepath;
    retcode* prev;
    retcode* next;
} retcode;

/**
 * A dynamic array to store pthread_ts which may be shared and safely 
 * interacted with from an arbitrary number of threads.
 *
 * Meant to be initialized and destroyed in a "parent" (main thread or 
 * similar), but may have its contents checked by retcode_ll_get and its 
 * underlying array contents appended to by retcode_ll_appendset in any 
 * thread.
 */
typedef struct retcode_ll_struct {
    uint32_t size;
    retcode* list;
    retcode* head;
    retcode* tail;
} retcode_ll;

retcode* node_init(thread_args* args);

/**
 * Create and initialize a new list on the heap according to capacity 
 * `new_capacity`. Includes creating and initializing the array of pthread_ts
 * within the list and the list's retcode_ll.
 *
 * Returns: valid heap pointer to a retcode_ll on success, or NULL on 
 * failure with errno set to ENOMEM from wrapped calloc() calls.
 */
retcode_ll* retcode_ll_init(size_t new_capacity);

/**
 * Inserts a new node at the tail 
 */
int retcode_ll_add(retcode_ll* list, retcode* node);

/**
 * Concatenate 
 */
retcode_ll* retcode_ll_concat(retcode_ll* dest, retcode_ll* src);

/**
 * "Poll" (join and get return value of) a particular thread whose pthread_t ID
 * is stored within a list's underlying array at `index`.
 *
 * Returns: return value of pthread_join (0 on success/errno on failure), or -1
 * if wrapped retcode_ll_get() call failed at that index, with errno 
 * "forwarded" appropriately (see retcode_ll_get() header).
 */
int retcode_ll_pollthread(retcode_ll* list, void** retval_ptr, size_t index);


void retcode_ll_destroy(retcode_ll* list);

#endif
