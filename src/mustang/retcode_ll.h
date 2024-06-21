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

typedef enum {
    SUCCESS = 0x0,
    ALLOC_FAILED, /* 0x1 */
    DIR_OPEN_FAILED, /* 0x2 */
    NEW_DIRFD_OPEN_FAILED = 0x4,
    THREADARG_FORK_FAILED = 0x8,
    PTHREAD_CREATE_FAILED = 0x10,
    PTHREAD_JOIN_FAILED = 0x20,
    CHILD_ALLOC_FAILED = 0x40
} RETCODE_FLAGS;

typedef struct retcode_struct retcode;

typedef struct retcode_struct {
    pthread_t self;
    RETCODE_FLAGS flags;
    char* basepath;
    retcode* prev;
    retcode* next;
} retcode;

typedef struct retcode_ll_struct {
    int size;
    retcode* head;
    retcode* tail;
} retcode_ll;

retcode* node_init(char* new_basepath, RETCODE_FLAGS new_flags);

/**
 * Create and initialize a new list on the heap according to capacity 
 * `new_capacity`. Includes creating and initializing the array of pthread_ts
 * within the list and the list's retcode_ll.
 *
 * Returns: valid heap pointer to a retcode_ll on success, or NULL on 
 * failure with errno set to ENOMEM from wrapped calloc() calls.
 */
retcode_ll* retcode_ll_init(void);

/**
 * Inserts a new node at the tail 
 */
int retcode_ll_add(retcode_ll* rll, retcode* node);

/**
 * Concatenate 
 */
retcode_ll* retcode_ll_concat(retcode_ll* dest, retcode_ll* src);

/**
 * Safely flush the contents of the retcode_ll `list` to the relevant logging 
 * file specified by `logfile` by surrounding write operations with locks and
 * unlocks on `logfile_lock`. Once writing is complete, empty (free) the space
 * associated with the retcode collection of `list`.
 */
void retcode_ll_flush(retcode_ll* rll, FILE* logfile, pthread_mutex_t* logfile_lock);

void retcode_ll_destroy(retcode_ll* rll);

#endif
