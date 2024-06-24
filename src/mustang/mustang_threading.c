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

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "mustang_threading.h"

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>
#define ID_MASK 0xFFFFFFFF
#define SHORT_ID() (pthread_self() & ID_MASK)
#endif

threadcount_verifier* verifier_init(size_t threads_max, pthread_mutex_t* new_lock, pthread_cond_t* new_cv) {
    threadcount_verifier* new_verifier = (threadcount_verifier*) calloc(1, sizeof(threadcount_verifier));

    if (new_verifier == NULL) {
        return NULL;
    }

    new_verifier->self_lock = new_lock;
    new_verifier->active_threads_cv = new_cv;
    new_verifier->active_threads = 0;
    new_verifier->max_threads = threads_max;

    return new_verifier;
}

void verifier_destroy(threadcount_verifier* verifier) {
    verifier->self_lock = NULL;
    verifier->active_threads_cv = NULL;
    free(verifier);
}

void active_threads_probe(threadcount_verifier* verifier) {
    pthread_mutex_lock(verifier->self_lock);

    while (verifier->active_threads >= verifier->max_threads) {
        pthread_cond_wait(verifier->active_threads_cv, verifier->self_lock);
    }

    verifier->active_threads += 1;
    pthread_mutex_unlock(verifier->self_lock);
}

void active_threads_vend(threadcount_verifier* verifier) {
    pthread_mutex_lock(verifier->self_lock);
    verifier->active_threads -= 1;
    pthread_cond_broadcast(verifier->active_threads_cv);
    pthread_mutex_unlock(verifier->self_lock);
}

thread_args* threadarg_init(hashtable* new_hashtable, pthread_mutex_t* new_ht_lock, char* new_basepath, 
        int new_fd, FILE* new_logfile, pthread_mutex_t* new_log_lock) {
    thread_args* new_args = (thread_args*) calloc(1, sizeof(thread_args));

    if ((new_args == NULL) || (errno == ENOMEM)) {
        return NULL;
    }

    new_args->hashtable = new_hashtable;
    new_args->hashtable_lock = new_ht_lock;
    new_args->basepath = new_basepath;
    new_args->cwd_fd = new_fd;
    new_args->log_ptr = new_logfile;
    new_args->log_lock = new_log_lock;

#ifdef DEBUG
    new_args->stdout_lock = NULL;
#endif

    return new_args;
}

thread_args* threadarg_fork(thread_args* existing, char* new_basepath, int new_fd) {
    thread_args* new_args = (thread_args*) calloc(1, sizeof(thread_args));

    if ((new_args == NULL) || (errno == ENOMEM)) {
        return NULL;
    }

    new_args->hashtable = existing->hashtable;
    new_args->hashtable_lock = existing->hashtable_lock;

    // TODO: (eventually) add in logic to dup/init new marfs_config and marfs_position for new thread

    new_args->basepath = new_basepath;
    new_args->cwd_fd = new_fd;
    new_args->log_ptr = existing->log_ptr;
    new_args->log_lock = existing->log_lock;

#ifdef DEBUG
    new_args->stdout_lock = existing->stdout_lock;
#endif

    return new_args;
}

void threadarg_destroy(thread_args* args) {
    args->basepath = NULL;
    args->log_ptr = NULL;

    // TODO: (eventually) add in code to set marfs_config and marfs_position struct pointers to NULL

    free(args);
}

