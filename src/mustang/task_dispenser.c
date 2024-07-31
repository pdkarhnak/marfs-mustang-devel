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

#include "task_dispenser.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

/**
 * Allocate space for, and return a pointer to, a new mustang_task struct on 
 * the heap. Initialize the task with all necessary state (MarFS config, MarFS
 * position, hashtable ref, hashtable lock, task dispenser ref, and function 
 * pointer indicating what routine to execute).
 *
 * Returns: valid pointer to mustang_task struct on success, or NULL on 
 * failure.
 */
mustang_task* task_init(marfs_config* task_config, marfs_position* task_position, hashtable* task_ht, pthread_mutex_t* task_ht_lock, task_dispenser* dispenser_ref, void (*traversal_routine)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*, task_dispenser*)) {
    mustang_task* new_task = (mustang_task*) calloc(1, sizeof(mustang_task));

    if (new_task == NULL) {
        return NULL;
    }

    new_task->config = task_config;
    new_task->position = task_position;
    new_task->ht = task_ht;
    new_task->ht_lock = task_ht_lock;
    new_task->dispenser_ptr = dispenser_ref;
    new_task->task_func = traversal_routine;
    new_task->next = NULL;

    return new_task;
}

/**
 * Allocate space for, and return a pointer to, a new task_dispenser struct on the 
 * heap according to a specified capacity.
 *
 * Returns: valid pointer to task_dispenser struct on success, or NULL on failure.
 *
 * NOTE: this function may return NULL under any of the following conditions:
 * - Failure to calloc() dispenser space 
 * - Failure to calloc() dispenser mutex
 * - pthread_mutex_init() failure for dispenser mutex
 * - Failure to calloc() space for at least one dispenser condition variable
 * - pthread_cond_init() failure for at least one dispenser condition variable
 */
task_dispenser* task_dispenser_init(size_t bucket_count) {
    if (bucket_count == 0) {
        errno = EINVAL;
        return NULL;
    }

    task_dispenser* new_dispenser = (task_dispenser*) malloc(sizeof(task_dispenser));

    if (new_dispenser == NULL) {
        return NULL;
    }

    pthread_mutex_t* new_manager_lock = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));

    if (pthread_mutex_init(new_manager_lock, NULL)) {
        free(new_manager_lock);
        free(new_dispenser);
        return NULL;
    }

    new_dispenser->manager_lock = new_manager_lock;

    mustang_task** new_buckets = (mustang_task**) calloc(bucket_count, sizeof(mustang_task*));

    if (new_buckets == NULL) {
        pthread_mutex_destroy(new_manager_lock);
        free(new_manager_lock);
        free(new_dispenser);
        return NULL;
    }
    
    new_dispenser->size = bucket_count;
    new_dispenser->task_buckets = new_buckets;
    new_dispenser->todos = 0;

    pthread_mutex_t* new_bucket_locks = (pthread_mutex_t*) calloc(bucket_count, sizeof(pthread_mutex_t));
    pthread_cond_t* new_cvs = (pthread_cond_t*) calloc(bucket_count, sizeof(pthread_cond_t));

    for (size_t bucket = 0; bucket < bucket_count; bucket += 1) {
        if (pthread_mutex_init(&(new_bucket_locks[bucket]), NULL)) {
            for (size_t prev_index = 0; prev_index != bucket; prev_index += 1) {
                pthread_mutex_destroy(&(new_bucket_locks[prev_index]));
                pthread_cond_destroy(&(new_cvs[prev_index]));
            } 
            free(new_bucket_locks);
            free(new_cvs);
            free(new_buckets);
            pthread_mutex_destroy(new_manager_lock);
            free(new_manager_lock);
            free(new_dispenser);
            return NULL;
        }

        if (pthread_cond_init(&(new_cvs[bucket]), NULL)) {
            for (size_t prev_index = 0; prev_index != bucket; prev_index += 1) {
                pthread_mutex_destroy(&(new_bucket_locks[prev_index]));
                pthread_cond_destroy(&(new_cvs[prev_index]));
            } 
            free(new_bucket_locks);
            free(new_cvs);
            free(new_buckets);
            pthread_mutex_destroy(new_manager_lock);
            free(new_manager_lock);
            free(new_dispenser);
            return NULL;
        }
    }

    pthread_cond_t* new_manager_cv = (pthread_cond_t*) calloc(1, sizeof(pthread_cond_t));

    if (new_manager_cv == NULL) {

        for (size_t bucket = 0; bucket < bucket_count; bucket += 1) {
            pthread_mutex_destroy(&(new_bucket_locks[bucket]));
            pthread_cond_destroy(&(new_cvs[bucket]));
        }

        free(new_cvs);
        free(new_bucket_locks);
        free(new_dispenser->task_buckets);
        pthread_mutex_destroy(new_manager_lock);
        free(new_manager_lock);
        free(new_dispenser);
        return NULL;
    }

    // If pthread_cond_init() fails...
    if (pthread_cond_init(new_manager_cv, NULL)) {
        free(new_manager_cv);

        for (size_t bucket = 0; bucket < bucket_count; bucket += 1) {
            pthread_mutex_destroy(&(new_bucket_locks[bucket]));
            pthread_cond_destroy(&(new_cvs[bucket]));
        }

        free(new_cvs);
        free(new_bucket_locks);
        free(new_dispenser->task_buckets);
        pthread_mutex_destroy(new_manager_lock);
        free(new_manager_lock);
        free(new_dispenser);
        return NULL;
    }

    new_dispenser->locks = new_bucket_locks;
    new_dispenser->bucket_cvs = new_cvs;
    new_dispenser->manager_cv = new_manager_cv;

    return new_dispenser;
}

/**
 * Atomically endispenser a new task `new_task` to the given task dispenser `dispenser`, 
 * adjusting internal dispenser state as necessary to reflect changes (new size, 
 * new head/tail nodes in dispenser, etc.).
 *
 * Returns: 0 on success, or -1 on failure with errno set to EINVAL (dispenser
 * is NULL).
 *
 * NOTE: this function wraps a pthread_cond_wait() loop on the dispenser's 
 * `space_available` cv field, so callers do not need to (and, for application
 * efficiency/to minimize lock contention, should not) separately lock the 
 * dispenser's lock and wait on the `space_available` cv.
 */
int task_push(task_dispenser* dispenser, mustang_task* new_task, size_t index) {
    if ((dispenser == NULL) || (new_task == NULL) || (index > dispenser->size)) {
        errno = EINVAL;
        return -1;
    }
    
    pthread_mutex_lock(dispenser->manager_lock);
    dispenser->todos += 1;
    pthread_mutex_unlock(dispenser->manager_lock);

    pthread_mutex_lock(&(dispenser->locks[index]));
    new_task->next = dispenser->task_buckets[index];
    dispenser->task_buckets[index] = new_task;
    pthread_cond_signal(&(dispenser->bucket_cvs[index]));
    pthread_mutex_unlock(&(dispenser->locks[index]));

    return 0;
}

/**
 * Atomically dedispenser (unlink) and return the `mustang_task` struct at the head
 * of the task dispenser `dispenser`, adjusting internal dispenser state as necessary to 
 * reflect changes (new size, head/tail nodes, etc.).
 *
 * Returns: valid pointer to mustang_task struct on success, or NULL on failure
 * with errno set (EINVAL for dispenser == NULL).
 *
 * NOTE: in a similar fashion to task_endispenser, this function wraps a 
 * pthread_cond_wait() loop on a dispenser condition variable (the task_available 
 * cv in this case) before returning. Callers do not need to separately wait, 
 * and, to keep lock contention to a minimum, should not.
 */
mustang_task* task_pop(task_dispenser* dispenser, size_t index) {
    if ((dispenser == NULL) || (index > dispenser->size)) {
        errno = EINVAL;
        return NULL;
    }

    mustang_task* retrieved_task;

    pthread_mutex_lock(&((dispenser->locks)[index]));

    while ((dispenser->task_buckets)[index] == NULL) {
        pthread_cond_wait(&(dispenser->bucket_cvs[index]), &(dispenser->locks[index]));
    }

    retrieved_task = (dispenser->task_buckets)[index];
    (dispenser->task_buckets)[index] = ((dispenser->task_buckets)[index])->next;
    pthread_mutex_unlock(&((dispenser->locks)[index]));
    
    retrieved_task->next = NULL;
    return retrieved_task;
}

/**
 * Destroy the given task_dispenser struct and free the memory associated with it.
 */
int task_dispenser_destroy(task_dispenser* dispenser) {

    for (size_t bucket_index = 0; bucket_index < dispenser->size; bucket_index += 1) {
        pthread_mutex_destroy(&(dispenser->locks)[bucket_index]);
        pthread_cond_destroy(&(dispenser->bucket_cvs)[bucket_index]);
    }

    free(dispenser->locks);
    free(dispenser->bucket_cvs);
    free(dispenser->task_buckets);
    pthread_cond_destroy(dispenser->manager_cv);
    free(dispenser->manager_cv);
    pthread_mutex_destroy(dispenser->manager_lock);
    free(dispenser->manager_lock);

    free(dispenser);
    return 0;
}
