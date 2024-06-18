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
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "mustang_threading.h"

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>

#define SHORT_ID() (pthread_self() & 0xFFFF)
#endif

threadcount_verifier* verifier_init(size_t threads_max) {
    threadcount_verifier* new_verifier = (threadcount_verifier*) calloc(1, sizeof(threadcount_verifier));

    if ((new_verifier == NULL) || (errno == ENOMEM)) {
        return NULL;
    }

    new_verifier->active_threads = 0;
    new_verifier->max_threads = threads_max;

    pthread_mutex_t* verifier_mutex = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(verifier_mutex, NULL);
    new_verifier->self_lock = verifier_mutex;

    pthread_cond_t* verifier_cv = (pthread_cond_t*) calloc(1, sizeof(pthread_cond_t));
    pthread_cond_init(verifier_cv, NULL);
    new_verifier->active_threads_cv = verifier_cv;

    return new_verifier;
}

void verifier_destroy(threadcount_verifier* verifier) {
    pthread_mutex_destroy(verifier->self_lock);
    free(verifier->self_lock);
    pthread_cond_destroy(verifier->active_threads_cv);
    free(verifier->active_threads_cv);
    free(verifier);
    verifier = NULL;
}

thread_args* threadarg_fork(thread_args* existing, char* new_basepath, int new_fd) {
    thread_args* new_args = (thread_args*) calloc(1, sizeof(thread_args));

    if ((new_args == NULL) || (errno == ENOMEM)) {
        return NULL;
    }

    new_args->tc_verifier = existing->tc_verifier;
    new_args->pt_vector = existing->pt_vector;
    new_args->hashtable = existing->hashtable;
    new_args->hashtable_lock = existing->hashtable_lock;

    // TODO: (eventually) add in logic to dup/init new marfs_config and marfs_position for new thread

    new_args->basepath = strdup(new_basepath);
    new_args->cwd_fd = new_fd;

#ifdef DEBUG
    new_args->stdout_lock = existing->stdout_lock;
#endif

    return new_args;
}

void threadarg_destroy(thread_args* args) {
    free(args->basepath);
    args->basepath = NULL;
    close(this_args->cwd_fd);

    // TODO: (eventually) add in code to set marfs_config and marfs_position struct pointers to NULL

    free(args);
}

void verify_active_threads(threadcount_verifier* verifier) {
    pthread_mutex_lock(verifier->self_lock);

    if ((verifier->active_threads) >= (verifier->max_threads)) {
        while (pthread_cond_wait(verifier->active_threads_cv, verifier->self_lock) != 0);
    }

    verifier->active_threads += 1;

    pthread_mutex_unlock(verifier->self_lock);
}

void signal_active_threads(threadcount_verifier* verifier) {
    pthread_mutex_lock(verifier->self_lock); 
    verifier->active_threads -= 1;
    pthread_mutex_unlock(verifier->self_lock);
    pthread_cond_broadcast(verifier->active_threads_cv);
}

void* thread_routine(void* args) {

    size_t retval = 0;
    thread_args* this_args = (thread_args*) args;

    verify_active_threads(this_args->tc_verifier);

#ifdef DEBUG
    char wd_buf[128];
    pthread_mutex_lock(this_args->stdout_lock);
    printf("[thread %0lx]: very beginning cwd: %s\n", SHORT_ID(), getcwd(wd_buf, 128));
    printf("[thread %0lx]: got basepath %s\n", SHORT_ID(), this_args->basepath);
    pthread_mutex_unlock(this_args->stdout_lock);
#endif

    DIR* cwd_handle = fdopendir(this_args->cwd_fd);

#ifdef DEBUG
    if (cd_code == -1) {
        pthread_mutex_lock(this_args->stdout_lock);
        printf("[thread %0lx]: failed to chdir (%s)\n", SHORT_ID(), strerror(errno));
        pthread_mutex_unlock(this_args->stdout_lock);
    }
#endif

#ifdef DEBUG
    char* cwd = getcwd(wd_buf, 128);
    pthread_mutex_lock(this_args->stdout_lock);
    printf("[thread %0lx]: cd'd into: %s\n", SHORT_ID(), cwd);
    pthread_mutex_unlock(this_args->stdout_lock);
#endif

    struct dirent* current_entry = readdir(pwd_handle);

    // Maintain a local buffer of pthread_ts to limit locking on the pthread_vector and "flush" pthread_ts in bulk
    pthread_t new_thread_ids[16];
    int pts_count = 0; // an index to keep track of how many pthread_ts to flush at one time

    while (current_entry != NULL) {
        if (current_entry->d_type == DT_DIR) {

            // Skip current directory "." and parent directory ".." to avoid infinite loop in directory traversal
            if ( (strcmp(current_entry->d_name, ".") == 0) || (strcmp(current_entry->d_name, "..") == 0) ) {
                current_entry = readdir(pwd_handle);
                continue;
            }

            if ((subdir_paths == NULL) || (errno == ENOMEM)) {
                // TODO: log error: ENOMEM
                return (void*) ENOMEM; // ENOMEM fail-deadly--can't do anything else
            }

            int next_cwd_fd = openat(this_args->cwd_fd, current_entry->d_name, O_RDONLY | O_DIRECTORY);

            thread_args* next_args = threadarg_fork(this_args, strdup(current_entry->d_name), next_cwd_fd);

            int createcode = pthread_create(&new_thread_ids[pts_count], NULL, &thread_routine, (void*) next_args);

            if (createcode != 0) {
                // TODO: log warning/error: EAGAIN (no system resources available/system-wide limit on threads encountered)
                // Not strictly fail-deadly for this thread (just new threads will not be spawned)
                retval = EAGAIN;
            } else {
                pts_count += 1;
            }

            // If 16 threads have been spawned from this thread, "flush" the 
            // local buffer and add the corresponding pthread_ts to the shared 
            // vector
            if (pts_count == 16) {
                int addcode = pthread_vector_appendset(this_args->pt_vector, new_thread_ids, 16);
                pts_count = 0; // Unconditionally reset the pts_count to zero. The flush either completely succeeds or completely fails.

                if (addcode != 0) {
                    retval = addcode;
                    // TODO: log error
                }
            }

#ifdef DEBUG
            pthread_mutex_lock(this_args->stdout_lock);
            printf("[thread %0lx]: forked new thread (ID: %0lx) at basepath %s\n", SHORT_ID(), (new_thread_ids[index] & 0xFFFF), subdir_paths[index]);
            pthread_mutex_unlock(this_args->stdout_lock);
#endif

        } else if (current_entry->d_type == DT_REG) {
#ifdef DEBUG
            pthread_mutex_lock(this_args->stdout_lock);
            printf("[thread %0lx]: recording file [%s]/'%s' in hashtable.\n", SHORT_ID(), this_args->basepath, current_entry->d_name);
            pthread_mutex_unlock(this_args->stdout_lock);
#endif

            pthread_mutex_lock(this_args->hashtable_lock);
            put(this_args->hashtable, current_entry->d_name);
            pthread_mutex_unlock(this_args->hashtable_lock);
        }

        current_entry = readdir(pwd_handle);
    }

    // Flush all remaining pthread_ts of spawned threads to the shared vector
    int pt_flushcode = pthread_vector_appendset(this_args->pt_vector, new_thread_ids, pts_count);

    if (pt_flushcode != 0) {
        retval = pt_flushcode;
        // TODO: log error
    }

    signal_active_threads(this_args->tc_verifier);
    threadarg_destroy(this_args);
    closedir(pwd_handle);

    return ((void*) retval);

}
