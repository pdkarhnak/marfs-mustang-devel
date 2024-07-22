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
#include <tagging/tagging.h>
#include <datastream/datastream.h>
#include "mustang_threading.h"
#include "task_queue.h"
#include "id_cache.h"

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#include "mustang_logging.h"

#ifdef DEBUG_MUSTANG
#define DEBUG DEBUG_MUSTANG
#elif (defined DEBUG_ALL)
#define DEBUG DEBUG_ALL
#endif

#define LOG_PREFIX "mustang_threading"
#include <logging/logging.h>

char* get_ftag(marfs_position* current_position, MDAL current_mdal, char* path) {
    MDAL_FHANDLE target_handle = current_mdal->open(current_position->ctxt, path, O_RDONLY);
    if (target_handle == NULL) {
        return NULL;
    }

    char* ftag_str = NULL;
    // 1 == hidden xattr
    ssize_t ftag_len = current_mdal->fgetxattr(target_handle, 1, FTAG_NAME, ftag_str, 0); // Fetch initial length of ftag

    if (ftag_len <= 0) {
        current_mdal->close(target_handle);
        errno = ENOATTR;
        return NULL;
    }

    ftag_str = (char*) calloc((ftag_len + 1), sizeof(char));

    if (ftag_str == NULL) {
        current_mdal->close(target_handle);
        errno = ENOMEM;
        return NULL; 
    }

    if (current_mdal->fgetxattr(target_handle, 1, FTAG_NAME, ftag_str, ftag_len) != ftag_len) {
        current_mdal->close(target_handle);
        free(ftag_str);
        errno = ESTALE;
        return NULL;
    }

    current_mdal->close(target_handle);
    return ftag_str;
}

void traverse_dir(marfs_config* base_config, marfs_position* task_position, hashtable* output_table, pthread_mutex_t* table_lock, task_queue* pool_queue) {
    id_cache* this_id_cache = id_cache_init(id_cache_capacity);

    if (this_id_cache == NULL) {
        LOG(LOG_ERR, "Failed to allocate memory! (%s)\n", strerror(errno));
        return;
    }

    // Attempt to fortify the thread's position (if not already fortified) and check for errors
    if ((task_position->ctxt == NULL) && config_fortifyposition(task_position)) {
        LOG(LOG_ERR, "Failed to fortify MarFS position!\n");
        return;
    }

    // Define a convenient alias for this thread's relevant MDAL, from which 
    // all metadata ops will be launched
    MDAL thread_mdal = task_position->ns->prepo->metascheme.mdal;

    // Recover a directory handle for the cwd to enable later readdir()
    MDAL_DHANDLE cwd_handle = thread_mdal->opendir(task_position->ctxt, ".");

    if (cwd_handle == NULL) {
        LOG(LOG_ERR, "Failed to open current directory for reading! (%s)\n", strerror(errno));
        config_abandonposition(task_position);
        free(task_position);
        return;
    }

    /** BEGIN directory traversal and file/directory discovery routine **/

    // "Regular" readdir logic
    struct dirent* current_entry = thread_mdal->readdir(cwd_handle);

    char* file_ftagstr = NULL;
    char* retrieved_id = NULL;

    while (current_entry != NULL) {
        // Ignore dirents corresponding to "invalid" paths (reference tree,
        // etc.)
        if (thread_mdal->pathfilter(current_entry->d_name) != 0) {
            current_entry = thread_mdal->readdir(cwd_handle); 
            continue; 
        }

        if (current_entry->d_type == DT_DIR) {

            // Skip current directory "." and parent directory ".." to avoid infinite loop in directory traversal
            if ( (strncmp(current_entry->d_name, ".", strlen(current_entry->d_name)) == 0) || (strncmp(current_entry->d_name, "..", strlen(current_entry->d_name)) == 0) ) {
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            marfs_position* new_dir_position = (marfs_position*) calloc(1, sizeof(marfs_position));
            if (new_dir_position == NULL) {
                LOG(LOG_ERR, "Failed to allocate memory for new new_task position (current entry: %s)\n", current_entry->d_name);
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            if (config_duplicateposition(task_position, new_dir_position)) {
                LOG(LOG_ERR, "Failed to duplicate parent position to new_task (current entry: %s)\n", current_entry->d_name);
                config_abandonposition(new_dir_position);
                free(new_dir_position);
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            char* new_basepath = strdup(current_entry->d_name);
            int new_depth = config_traverse(base_config, new_dir_position, &new_basepath, 0);

            if (new_depth < 0) {
                LOG(LOG_ERR, "Failed to traverse to target: \"%s\"\n", current_entry->d_name);

                free(new_basepath);
                config_abandonposition(new_dir_position);
                free(new_dir_position);
                
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }
            
            MDAL_DHANDLE next_cwd_handle = thread_mdal->opendir(new_dir_position->ctxt, current_entry->d_name);

            if (next_cwd_handle == NULL) {
                LOG(LOG_ERR, "Failed to open directory handle for new_task (%s) (directory: \"%s\")\n", strerror(errno), current_entry->d_name);

                free(new_basepath);
                config_abandonposition(new_dir_position);
                free(new_dir_position);

                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            if (thread_mdal->chdir(new_dir_position->ctxt, next_cwd_handle)) {
                LOG(LOG_ERR, "Failed to chdir to target directory \"%s\" (%s).\n", current_entry->d_name, strerror(errno));

                free(new_basepath);
                config_abandonposition(new_dir_position);
                free(new_dir_position);
                thread_mdal->closedir(next_cwd_handle);

                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            new_dir_position->depth = new_depth;

            // depth == -1 case (config_traverse() error) has already been handled, so presuming that 0 and > 0 are exhaustive is safe.
            switch (new_depth) {
                case 0:
                    mustang_task* new_ns_task = task_init(base_config, new_dir_position, output_table, table_lock, pool_queue, &traverse_ns);
                    task_enqueue(pool_queue, new_ns_task);
                    LOG(LOG_DEBUG, "Created new task to traverse namespace \"%s\"\n", new_basepath);
                    break;
                default:
                    mustang_task* new_dir_task = task_init(base_config, new_dir_position, output_table, table_lock, pool_queue, &traverse_dir);
                    task_enqueue(pool_queue, new_dir_task);
                    LOG(LOG_DEBUG, "Created new task to traverse directory \"%s\"\n", new_basepath);
                    break;
            }

            // Basepath no longer needed after logging since new_task reopens 
            // dirhandle not with basepath, but with post-chdir "." reference.
            free(new_basepath); 

        } else if (current_entry->d_type == DT_REG) {
            file_ftagstr = get_ftag(task_position, thread_mdal, current_entry->d_name);
            FTAG retrieved_tag = {0};
            
            if (ftag_initstr(&retrieved_tag, file_ftagstr)) {
                LOG(LOG_ERR, "Failed to initialize FTAG for file: \"%s\"\n", current_entry->d_name);
                free(file_ftagstr);
                current_entry = thread_mdal->readdir(cwd_handle);
                continue;
            }

            size_t objno_min = retrieved_tag.objno;
            size_t objno_max = datastream_filebounds(&retrieved_tag);
            ne_erasure placeholder_erasure;
            ne_location placeholder_location;

            for (size_t i = objno_min; i <= objno_max; i += 1) {
                retrieved_tag.objno = i;
                if (datastream_objtarget(&retrieved_tag, &(task_position->ns->prepo->datascheme), &retrieved_id, &placeholder_erasure, &placeholder_location)) { 
                    LOG(LOG_ERR, "Failed to get object ID for chunk %zu of current object \"%s\"\n", i, current_entry->d_name);
                    continue;
                }

                // A small optimization: minimize unnecessary locking by simply ignoring known duplicate object IDs
                // and not attempting to add them to the hashtable again.                    
                if (id_cache_probe(this_id_cache, retrieved_id) == 0) {
                    id_cache_add(this_id_cache, retrieved_id);
                    pthread_mutex_lock(table_lock);
                    put(output_table, retrieved_id); // put() dupes string into new heap space
                    pthread_mutex_unlock(table_lock);
                    LOG(LOG_DEBUG, "Recorded object \"%s\" in hashtable.\n", retrieved_id);
                }

                free(retrieved_id);
                retrieved_id = NULL; // make ptr NULL to better discard stale reference
            }

            ftag_cleanup(&retrieved_tag); // free internal allocated memory for FTAG's ctag and streamid fields
            free(file_ftagstr);
            file_ftagstr = NULL; // discard stale reference to FTAG to prevent double-free
        }

        current_entry = thread_mdal->readdir(cwd_handle);
    }

    // if FTAG cleanup somehow failed, ensure cleanup instead occurs here
    if (file_ftagstr != NULL) {
        free(file_ftagstr);
        file_ftagstr = NULL;
    }

    // string pointer should have been made null and cleaned up---check that this has occurred
    if (retrieved_id != NULL) {
        free(retrieved_id);
        retrieved_id = NULL;
    }

    // Clean up other per-task state
    if (thread_mdal->closedir(cwd_handle)) {
        LOG(LOG_WARNING, "Failed to close handle for current working directory!\n");
    }

    id_cache_destroy(this_id_cache);
    
    if (config_abandonposition(task_position)) {
        LOG(LOG_WARNING, "Failed to abandon base position!\n");
    }

    free(task_position);
}

void traverse_ns(marfs_config* base_config, marfs_position* task_position, hashtable* output_table, pthread_mutex_t* table_lock, task_queue* pool_queue) {
    // Attempt to fortify the thread's position (if not already fortified) and check for errors
    if ((task_position->ctxt == NULL) && config_fortifyposition(task_position)) {
        LOG(LOG_ERR, "Failed to fortify MarFS position!\n");
        return;
    }

    // Namespaces may contain other namespaces. Examine first the subspace list
    // for the current NS to see if tasks to traverse subspaces need to be 
    // enqueued.
    if (task_position->ns->subnodes) {

        for (size_t subnode_index = 0; subnode_index < task_position->ns->subnodecount; subnode_index += 1) {
            HASH_NODE current_subnode = (task_position->ns->subnodes)[subnode_index];

            marfs_position* new_ns_position = (marfs_position*) calloc(1, sizeof(marfs_position));

            if (config_duplicateposition(task_position, new_ns_position)) {
                LOG(LOG_ERR, "Failed to duplicate position for new new_task thread\n");
                free(new_ns_position);
                continue;
            }
            
            char* new_ns_path = strdup(current_subnode.name);
            if (config_traverse(base_config, new_ns_position, &new_ns_path, 0)) {
                LOG(LOG_ERR, "Failed to traverse to new new_task position: %s\n", current_subnode.name);
                free(new_ns_path);
                config_abandonposition(new_ns_position);
                free(new_ns_position);
                continue;
            }

            // subspaces of this namespace are namespaces "prima facie" --- automatically create traverse_ns task
            mustang_task* new_ns_task = task_init(base_config, new_ns_position, output_table, table_lock, pool_queue, &traverse_ns);
            task_enqueue(pool_queue, new_ns_task);
            LOG(LOG_DEBUG, "Created new namespace traversal task at basepath: \"%s\"\n", new_ns_path);
            free(new_ns_path);
        }
    }

    // Namespaces may also contain subdirectories and files. Proceed to the 
    // directory traversal routine (the "regular" common case) and examine any
    // other namespace contents.
    traverse_dir(base_config, task_position, output_table, table_lock, pool_queue);
}

void* thread_launcher(void* args) {
    task_queue* queue = (task_queue*) args;
    errno = 0; // Since errno not guaranteed to be zero-initialized

    while (1) {
        // Wraps a wait on a task being available in the queue (a cv wait), so
        // this function only returns when a task is successfully and 
        // atomically acquired.
        mustang_task* next_task = task_dequeue(queue);

        if (next_task->task_func == NULL) {
            // Special sentinel condition for parent to tell pooled threads "no more work to do".
            free(next_task);
            return NULL;
        }

        // In all other circumstances, tasks will be initialized with a
        // function pointer indicating what work (traversing a namespace or
        // traversing a directory) needs to be performed, so jump to that.
        next_task->task_func(next_task->config, next_task->position, next_task->ht, next_task->ht_lock, queue);

        pthread_mutex_lock(queue->lock); 
        
        // Decrement the number of tasks to do *after* the task execution has
        // returned (even when failing) so that the parent is not signaled to
        // clean up state while threads may be using it in tasks.
        queue->todos -= 1;
        LOG(LOG_DEBUG, "Queue todos left: %zu\n", queue->todos);

        if (queue->todos == 0) {
            // If no other tasks to do, signal the parent to wake up and clean 
            // up shared state.
            pthread_cond_signal(queue->manager_cv);
        }

        pthread_mutex_unlock(queue->lock);

        // Task functions already abandon the position and free the associated
        // memory. Other state (config, hashtable + lock, etc.) is held by the
        // parent and will be cleaned up there. So, just free the task struct 
        // itself.
        free(next_task);
    }

    return NULL;
}
