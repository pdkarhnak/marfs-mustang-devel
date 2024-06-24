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
#include "pthread_vector.h"
#include "retcode_ll.h"

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>
#define ID_MASK 0xFFFFFFFF
#define SHORT_ID() (pthread_self() & ID_MASK)
#endif

void* thread_main(void* args) {

    thread_args* this_args = (thread_args*) args;

    retcode* this_retcode = node_init(this_args->basepath, SUCCESS);
    retcode_ll* this_ll = retcode_ll_init();

    if ((this_retcode) == NULL || (this_ll == NULL)) {
        threadarg_destroy(this_args);
        return NULL; // Will be interpreted as flag CHILD_ALLOC_FAILED
    }

    marfs_position local_position = { .ns = NULL, .depth = 0 };

    if (config_duplicateposition(this_args->thread_position, &local_position)) {
        this_retcode->flags |= CHILD_DUPPOS_FAILED;
        threadarg_destroy(this_args);
        return NULL;
    }

    DIR* cwd_handle = fdopendir(this_args->cwd_fd);

    if (cwd_handle == NULL) {
        this_retcode->flags |= DIR_OPEN_FAILED;
        retcode_ll_add(this_ll, this_retcode);

#ifdef DEBUG
        pthread_mutex_lock(this_args->stdout_lock);
        printf("ERROR: directory handle is NULL! (%s)\n", strerror(errno));
        pthread_mutex_unlock(this_args->stdout_lock);
#endif

        threadarg_destroy(this_args);
        return (void*) this_ll;
    }

    pthread_vector* spawned_threads = pthread_vector_init(DEFAULT_CAPACITY);

    if (spawned_threads == NULL) {
        this_retcode->flags |= ALLOC_FAILED;
        retcode_ll_add(this_ll, this_retcode);
        closedir(cwd_handle);
        threadarg_destroy(this_args);
        return (void*) this_ll;
    }

    struct dirent* current_entry = readdir(cwd_handle);

    while (current_entry != NULL) {
        if (current_entry->d_type == DT_DIR) {

            // Skip current directory "." and parent directory ".." to avoid infinite loop in directory traversal
            if ( (strncmp(current_entry->d_name, ".", strlen(current_entry->d_name)) == 0) || (strncmp(current_entry->d_name, "..", strlen(current_entry->d_name)) == 0) ) {
                current_entry = readdir(cwd_handle);
                continue;
            }
  
            int next_cwd_fd = openat(this_args->cwd_fd, current_entry->d_name, O_RDONLY | O_DIRECTORY);

            if (next_cwd_fd == -1) {
                this_retcode->flags |= NEW_DIRFD_OPEN_FAILED;
                current_entry = readdir(cwd_handle);
                continue;
            }

            thread_args* next_args = threadarg_fork(this_args, strndup(current_entry->d_name, strlen(current_entry->d_name)), next_cwd_fd);

            if (next_args == NULL) {
                this_retcode->flags |= THREADARG_FORK_FAILED;
                close(next_cwd_fd);
                current_entry = readdir(cwd_handle);
                continue;
            }

            pthread_t next_id;
            int createcode = pthread_create(&next_id, NULL, &thread_main, (void*) next_args);

            if (createcode != 0) {
                // Not strictly fail-deadly for this thread (just new threads will not be spawned)
                this_retcode->flags |= PTHREAD_CREATE_FAILED;
                close(next_cwd_fd);
            } else {
                pthread_vector_append(spawned_threads, next_id);
            } 
#ifdef DEBUG
            if (createcode == 0) {
                pthread_mutex_lock(this_args->stdout_lock);
                printf("[thread %0lx]: forked new thread (ID: %0lx) at basepath %s\n", 
                        SHORT_ID(), (next_id & 0xFFFFFFFF), current_entry->d_name);
                pthread_mutex_unlock(this_args->stdout_lock);
            }
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

        current_entry = readdir(cwd_handle);
    }

    pthread_t to_join;

    for (int thread_index = 0; thread_index < spawned_threads->size; thread_index += 1) {

        int findcode = at_index(spawned_threads, thread_index, &to_join);

        if (findcode == -1) {
            continue;
        }

        retcode_ll* joined_ll; 
        int joincode = pthread_join(to_join, (void**) &joined_ll);

        if (joincode != 0) {
            this_retcode->flags |= PTHREAD_JOIN_FAILED;
            continue;
        }

        if (joined_ll == NULL) {
            this_retcode->flags |= CHILD_ALLOC_FAILED;
            continue;
        }

        this_ll = retcode_ll_concat(this_ll, joined_ll);

        if (this_ll->size >= RC_LL_LEN_MAX) {
            retcode_ll_flush(this_ll, this_args->log_ptr, this_args->log_lock);
        }

    }

    retcode_ll_add(this_ll, this_retcode);

    pthread_vector_destroy(spawned_threads);

    threadarg_destroy(this_args);
    closedir(cwd_handle);

    return (void*) this_ll;

}
