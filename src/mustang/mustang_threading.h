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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <config/config.h>
#include <mdal/mdal.h>
#include "hashtable.h"
#include "retcode_ll.h"

extern void* thread_main(void* args);

typedef struct thread_args_struct thread_args;

typedef struct thread_args_struct {

    // MarFS context components for this thread: position and config
    marfs_config* base_config;
    marfs_position* base_position;

    // Synchronization for the output hashtable of object names
    hashtable* hashtable;
    pthread_mutex_t* hashtable_lock;

    // After a verify_active_threads() call to put the thread to sleep as
    // needed until room is available, This will be the path that the new
    // thread opens.
    char* basepath; 

    pthread_mutex_t* log_lock;


} thread_args;

/**
 * Initialize a new argument struct in preparation for the creation of a new
 * thread. This creates a thread_args struct "from scratch" and is intended to
 * be used in the "top-level" thread (the thread which runs main). Due to the 
 * considerable amount of shared state between all threads, threadarg_fork() 
 * is used as documented below for all other thread creation occurring in 
 * threads besides the top-level thread.
 */
thread_args* threadarg_init(marfs_config* shared_config, marfs_position* shared_position, hashtable* new_hashtable, 
        pthread_mutex_t* new_ht_lock, char* new_basepath, pthread_mutex_t* new_log_lock);

/** 
 * Given a thread's arguments and new inputs for thread marfs_position and
 * basepath, create a new argument struct for a new thread, then create a new 
 * thread and record struct allocation/thread creation results accordingly.
 *
 * Returns: 
 * - RETCODE_FLAGS indicating whether argument struct allocation or 
 *   thread creation failed. 
 * - thread_id "by reference": pthread_create called on thread_id such that, if
 *   RETCODE_FLAGS == RETCODE_SUCCESS, *thread_id contains a valid pthread_t on 
 *   return which may be joined.
 */
RETCODE_FLAGS mustang_spawn(thread_args* existing, pthread_t* thread_id, marfs_position* new_position, char* new_basepath);

/**
 * Destroy a thread's arguments at the conclusion of a thread's run. This must
 * be called after mustang_spawn() is called for all applicable new threads 
 * which will traverse encountered subdirectories.
 */
int threadarg_destroy(thread_args* args);

char* get_ftag(marfs_position* current_position, MDAL current_mdal, char* path);

#endif
