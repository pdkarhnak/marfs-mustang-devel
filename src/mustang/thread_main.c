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
#include <limits.h>
#include <mdal/mdal.h>
#include <tagging/tagging.h>
#include <datastream/datastream.h>
#include <ne/ne.h>
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
    errno = 0;

    thread_args* this_args = (thread_args*) args;

    retcode* this_retcode = node_init(this_args->basepath, RETCODE_SUCCESS);
    retcode_ll* this_ll = retcode_ll_init();

    if ((this_retcode) == NULL || (this_ll == NULL)) {
        threadarg_destroy(this_args);
        return NULL; // Will be interpreted as flag CHILD_ALLOC_FAILED
    }

    marfs_position* thread_position = this_args->base_position;

    // Attempt to fortify the thread's position (if not already fortified) and check for errors
    if ((thread_position->ctxt == NULL) && config_fortifyposition(thread_position)) {
        this_retcode->flags |= FORTIFYPOS_FAILED;
        retcode_ll_add(this_ll, this_retcode);
        threadarg_destroy(this_args);
        return (void*) this_ll;
    }

    // Define a convenient alias for this thread's relevant MDAL, from which 
    // all metadata ops will be launched
    MDAL thread_mdal = thread_position->ns->prepo->metascheme.mdal;

    // Recover a directory handle for the cwd to enable later readdir()
    MDAL_DHANDLE cwd_handle = thread_mdal->opendir(thread_position->ctxt, ".");

    if (cwd_handle == NULL) {
        // TODO: log and continue/set some internal flag to skip readdir() calls after namespace checks
    }

    pthread_vector* spawned_threads = pthread_vector_init(DEFAULT_CAPACITY);

    if (spawned_threads == NULL) {
        this_retcode->flags |= ALLOC_FAILED;
        retcode_ll_add(this_ll, this_retcode);
        threadarg_destroy(this_args);
        return (void*) this_ll;
    }

    // TODO: add namespace checks

    if (thread_position->depth == 0) {
        // check reference chase for position struct's corresponding namespace (and list of subspaces within namespace)
        /* thread_position->ns->subnodes */
        if (thread_position->ns->subnodes) {
            for (size_t subnode_index = 0; subnode_index < thread_position->ns->subnodecount; subnode_index += 1) {
                HASH_NODE current_subnode = (thread_position->ns->subnodes)[subnode_index];

                marfs_position* child_ns_position = (marfs_position*) calloc(1, sizeof(marfs_position));

                if (config_duplicateposition(thread_position, child_ns_position)) {
                    // TODO: clean up and continue
                }
                
                char* child_ns_path = strdup(current_subnode.name);
                if (config_traverse(this_args->base_config, child_ns_position, &child_ns_path, 0)) {
                    // TODO: log, clean up, and continue
                    free(child_ns_path);
                    config_abandonposition(child_ns_position);
                    continue;
                }

                pthread_t next_ns_thread;
                RETCODE_FLAGS ns_spawn_flags = mustang_spawn(this_args, &next_ns_thread, child_ns_position, child_ns_path);

                if (ns_spawn_flags != RETCODE_SUCCESS) {
                    this_retcode->flags |= ns_spawn_flags;
                }
            }
        }
    }

    // "Regular" readdir logic
    struct dirent* current_entry = thread_mdal->readdir(cwd_handle);

    char* file_ftagstr = NULL;
    char* retrieved_id = NULL;

    while (current_entry != NULL) {
        if (current_entry->d_type == DT_DIR) {

            // Skip current directory "." and parent directory ".." to avoid infinite loop in directory traversal
            if ( (strncmp(current_entry->d_name, ".", strlen(current_entry->d_name)) == 0) || (strncmp(current_entry->d_name, "..", strlen(current_entry->d_name)) == 0) ) {
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            marfs_position* child_position = (marfs_position*) calloc(1, sizeof(marfs_position));
            if (child_position == NULL) {
                this_retcode->flags |= ALLOC_FAILED;
                // add cleanup logic: close dir handle, etc.
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            if (config_duplicateposition(thread_position, child_position)) {
                this_retcode->flags |= DUPPOS_FAILED; // or something like that
                // add cleanup logic: close dir handle, etc.
                continue;
            }

            char* new_basepath = strdup(current_entry->d_name);

            // TODO: verify alternating usage of new_basepath and current_entry->d_name "directly" here
            int new_depth = config_traverse(this_args->base_config, child_position, &new_basepath, 0);

            if (new_depth < 0) {
                // TODO: log, clean up, and continue
            }
            
            MDAL_DHANDLE next_cwd_handle = thread_mdal->opendir(child_position->ctxt, current_entry->d_name);

            if (thread_mdal->chdir(child_position->ctxt, next_cwd_handle)) {
                // TODO: log, clean up, and continue
            }

            child_position->depth = new_depth;

            pthread_t next_id;
            RETCODE_FLAGS spawn_flags = mustang_spawn(this_args, &next_id, child_position, new_basepath);

            if (spawn_flags != RETCODE_SUCCESS) {
                this_retcode->flags |= spawn_flags;
            } else {
                pthread_vector_append(spawned_threads, next_id);
#ifdef DEBUG
                pthread_mutex_lock(this_args->stdout_lock);
                printf("[thread %0lx]: forked new thread (ID: %0lx) at basepath %s\n", 
                        SHORT_ID(), (next_id & 0xFFFFFFFF), current_entry->d_name);
                pthread_mutex_unlock(this_args->stdout_lock);
#endif
            }


        } else if (current_entry->d_type == DT_REG) {
#ifdef DEBUG
            pthread_mutex_lock(this_args->stdout_lock);
            printf("[thread %0lx]: recording file [%s]/'%s' in hashtable.\n", SHORT_ID(), this_args->basepath, current_entry->d_name);
            pthread_mutex_unlock(this_args->stdout_lock);
#endif

            file_ftagstr = get_ftag(thread_position, thread_mdal, current_entry->d_name);
            FTAG retrieved_tag = {0};
            
            if (ftag_initstr(&retrieved_tag, file_ftagstr)) {
                this_retcode->flags |= FTAG_INIT_FAILED;
                free(file_ftagstr);
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            // TODO: calculate object bounds and iterate accordingly
            size_t objno_min = retrieved_tag.objno;
            size_t objno_max = datastream_filebounds(retrieved_tag);
            retrieved_id = (char*) calloc(PATH_MAX, sizeof(char));
            ne_erasure placeholder_erasure;
            ne_location placeholder_location;

            // TODO: implement namecache optimization at some point
            for (size_t i = objno_min; i <= objno_max; i += 1) {
                retrieved_tag.objno = i;
                if (datastream_objtarget(&retrieved_tag, &(thread_position->ns->prepo->datascheme), &retrieved_id, &placeholder_erasure, &placeholder_location)) {

                    // TODO: clean up this iteration, readdir, and continue
                }

                pthread_mutex_lock(this_args->hashtable_lock);
                put(this_args->hashtable, retrieved_id);
                pthread_mutex_unlock(this_args->hashtable_lock);
                retrieved_id = memset(retrieved_id, 0, PATH_MAX);
            }

            free(file_ftagstr);
            free(retrieved_id);
        }

        current_entry = thread_mdal->readdir(cwd_handle);
    }

    /* --- begin join and cleanup --- */
    if (file_ftagstr != NULL) {
        free(file_ftagstr);
    }

    if (retrieved_id != NULL) {
        free(retrieved_id);
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

    if (thread_mdal->closedir(cwd_handle)) {
        this_retcode->flags |= CLOSEDIR_FAILED;
    }

    retcode_ll_add(this_ll, this_retcode);

    pthread_vector_destroy(spawned_threads);

    threadarg_destroy(this_args);
    thread_position = NULL;

    return (void*) this_ll;

}
