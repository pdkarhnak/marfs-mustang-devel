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

#ifndef __MUSTANG_THREADING_H__
#define __MUSTANG_THREADING_H__

#include "hashtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct threadcount_verifier_struct threadcount_verifier;

typedef struct threadcount_verifier_struct {
    pthread_mutex_t* self_lock;
    pthread_cond_t* active_threads_cv;
    size_t active_threads;
    size_t max_threads;
} threadcount_verifier;

typedef struct thread_args_struct thread_args;

typedef struct thread_args_struct {

    // MarFS context components for this thread: position and config
    /*
    marfs_position* this_position;
    marfs_config* this_config;
    */

    // Synchronization for the output hashtable of object names
    hashtable* hashtable;
    pthread_mutex_t* hashtable_lock;

    // After a verify_active_threads() call to put the thread to sleep as
    // needed until room is available, This will be the path that the new
    // thread opens.
    char* basepath; 

    // Parent-created file descriptor to properly isolate child's new cwd
    int cwd_fd;

    FILE* log_ptr;
    pthread_mutex_t* log_lock;

#ifdef DEBUG
    pthread_mutex_t* stdout_lock;
#endif

} thread_args;

/**
 * Allocate a threadcount_verifier struct (see above) which threads will check
 * to verify that their work does not overwhelm the specified maximum number of
 * active threads at a time.
 *
 * This should be called once in the thread which runs main() for an entire
 * `mustang` process.
 */
threadcount_verifier* verifier_init(size_t threads_max, pthread_mutex_t* new_lock, pthread_cond_t* new_cv);

/**
 * Destroy a threadcount_verifier struct and its contents. This should be 
 * called once in the thread which runs main() for an entire `mustang` process,
 * and all "child" threads performing traversal must first be verified to have 
 * exited via pthread_join() calls in the parent.
 */
void verifier_destroy(threadcount_verifier* verifier);

/**
 * Given a specific threadcount_verifier struct with a lock and cv, synchronize
 * on the cv and verifier state to ensure that a thread's "active" operation 
 * does not exceed the program limit retrieved from the command-line arguments.
 */
void active_threads_probe(threadcount_verifier* verifier);

/**
 * Given a threadcount_verifier struct, broadcast on the verifier's cv to let 
 * other threads wake up (space permissive relative to the thread limit) and 
 * start "active" operation by returning from active_threads_probe().
 */
void active_threads_vend(threadcount_verifier* verifier);

/**
 * Initialize a new argument struct in preparation for the creation of a new
 * thread. This creates a thread_args struct "from scratch" and is intended to
 * be used in the "top-level" thread (the thread which runs main). Due to the 
 * considerable amount of shared state between all threads, threadarg_fork() 
 * is used as documented below for all other thread creation occurring in 
 * threads besides the top-level thread.
 */
thread_args* threadarg_init(hashtable* new_hashtable, pthread_mutex_t* new_ht_lock, char* new_basepath, 
        int new_fd, FILE* new_logfile, pthread_mutex_t* new_log_lock);

/**
 * "fork" a thread's arguments in preparation for the creation of a new thread
 * which will traverse a layer below the caller thread in the directory 
 * hierarchy. Technically a misnomer since fork(2) is never called; this
 * function merely duplicates most of the current's threads arguments to set
 * up shared state with a new thread.
 */
thread_args* threadarg_fork(thread_args* existing, char* new_basepath, int new_fd);

/**
 * Destroy a thread's arguments at the conclusion of a thread's run. This must
 * be called after threadarg_fork() is called for all applicable new threads 
 * which will traverse encountered subdirectories.
 */
void threadarg_destroy(thread_args* args);

#endif
