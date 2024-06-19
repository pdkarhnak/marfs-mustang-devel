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

#ifndef THREADBUF_SIZE
#define THREADBUF_SIZE 16
#endif

#ifndef NAMECACHE_SIZE
#define NAMECACHE_SIZE 8
#endif

#include "hashtable.h"
#include "pthread_vector.h"
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
    // Synchronization components to enforce a maximum number of active threads
    // at one time.
    threadcount_verifier* tc_verifier;

    pthread_vector* pt_vector;

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
    // thread chdir()-s into.
    char* basepath; 

    // Parent-created file descriptor to properly isolate child's new cwd
    int cwd_fd;

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
threadcount_verifier* verifier_init(size_t threads_max);

/**
 * Destroy a threadcount_verifier struct and its contents. This should be 
 * called once in the thread which runs main() for an entire `mustang` process,
 * and all "child" threads performing traversal must first be verified to have 
 * exited via pthread_join() calls in the parent (or `mustang` API calls like 
 * pthread_vector_pollthread()).
 */
void verifier_destroy(threadcount_verifier* verifier);

/**
 * Initialize a new argument struct in preparation for the creation of a new
 * thread. This creates a thread_args struct "from scratch" and is intended to
 * be used in the "top-level" thread (the thread which runs main). Due to the 
 * considerable amount of shared state between all threads, threadarg_fork() 
 * is used as documented below for all other thread creation occurring in 
 * threads besides the top-level thread.
 */
thread_args* threadarg_init(threadcount_verifier* new_verifier, pthread_vector* new_vector, 
        hashtable* new_hashtable, pthread_mutex_t* new_ht_lock, char* new_basepath, int new_fd);

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

/**
 * A wrapper for threads to synchronize based on the state of a threadcount
 * verifier (namely, the verifier's mutex and cv) and ensure that the number
 * of active threads (or, specifically, the number of threads performing 
 * "actual" traversal work) does not overwhelm the application argument for
 * the maximum number of active threads.
 */
void verify_active_threads(threadcount_verifier* verifier);

/**
 * A wrapper for threads to synchronize on a verifier; decrement the number
 * of active threads to indicate one fewer thread is performing "actual" 
 * traversal work; and broadcast on the verifier's condition variable to allow
 * waiting threads to wake up, return from verify_active_threads(), and begin
 * "actual" traversal work.
 */
void signal_active_threads(threadcount_verifier* verifier);

#endif
