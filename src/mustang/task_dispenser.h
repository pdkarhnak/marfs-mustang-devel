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

#ifndef __MUSTANG_TASK_QUEUE_H__
#define __MUSTANG_TASK_QUEUE_H__

#include <stdlib.h>
#include <pthread.h>
#include <config/config.h>
#include "hashtable.h"

typedef struct mustang_task_struct mustang_task;

typedef struct mustang_task_dispenser_struct task_dispenser;

typedef struct mustang_task_struct {
    marfs_config* config;
    marfs_position* position;
    hashtable* ht;
    pthread_mutex_t* ht_lock;
    task_dispenser* dispenser_ptr; // Tasks are retrieved from the dispenser, but during task execution other tasks may need to be dispenser.
    // The routine to execute. For the current version of Mustang (1.2.0), either `traverse_ns()` for a namespace or `traverse_dir()` for a regular directory.
    // If NULL, workers will detect this, clean up their state, and exit.
    void (*task_func)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*, task_dispenser*);
    mustang_task* next; // Each "bucket" within dispenser a singly-linked list of tasks
} mustang_task;

typedef struct mustang_task_dispenser_struct {
    size_t size;
    size_t todos;
    mustang_task** task_buckets;
    pthread_mutex_t* locks; // Makes any interactions with dispenser state (tasks in dispenser, size, etc.) atomic.
    pthread_cond_t* bucket_cvs;
    pthread_mutex_t* manager_lock;
    pthread_cond_t* manager_cv; // The synchronization point between manager and workers to indicate whether all work is finished.
} task_dispenser;

/**
 * Allocate space for, and return a pointer to, a new mustang_task struct on 
 * the heap. Initialize the task with all necessary state (MarFS config, MarFS
 * position, hashtable ref, hashtable lock, task dispenser ref, and function 
 * pointer indicating what routine to execute).
 *
 * Returns: valid pointer to mustang_task struct on success, or NULL on 
 * failure.
 */
mustang_task* task_init(marfs_config* task_config, marfs_position* task_position, hashtable* task_ht, pthread_mutex_t* task_ht_lock, task_dispenser* task_dispenser_ref, void (*traversal_routine)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*, task_dispenser*));

/**
 * Allocate space for, and return a pointer to, a new task_dispenser struct on the 
 * heap according to a specified capacity.
 *
 * Returns: valid pointer to task_dispenser struct on success, or NULL on failure.
 *
 * NOTE: this function may return NULL under any of the following conditions:
 * - Failure to calloc() dispenser space 
 * - Failure to calloc() any of the dispenser's bucket mutexes
 * - pthread_mutex_init() failure for any of the dispenser's bucket mutexes
 * - Failure to calloc() space for the manager condition variable
 * - pthread_cond_init() failure for the manager condition variable
 */
task_dispenser* task_dispenser_init(size_t new_capacity);

/**
 * TODO: document
 */
int task_push(task_dispenser* dispenser, mustang_task* new_task, size_t index);

/**
 * TODO: document
 */
mustang_task* task_pop(task_dispenser* dispenser, size_t index);

/**
 * Destroy the given task_dispenser struct and free the memory associated with it.
 */
int task_dispenser_destroy(task_dispenser* dispenser);

#endif
