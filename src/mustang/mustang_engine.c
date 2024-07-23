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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <marfs.h>
#include <config/config.h>
#include <datastream/datastream.h>

#include "mustang_logging.h"

#ifdef DEBUG_MUSTANG
#define DEBUG DEBUG_MUSTANG
#elif (defined DEBUG_ALL)
#define DEBUG DEBUG_ALL
#endif

#define LOG_PREFIX "mustang_engine"
#include <logging/logging.h>

#include "hashtable.h"
#include "mustang_threading.h"
#include "task_queue.h"

#define HC_MAX ((size_t) 1 << 63)

extern void* thread_launcher(void* args);

size_t id_cache_capacity;

int main(int argc, char** argv) {
    errno = 0; // to guarantee an initially successful context and avoid "false positive" errno settings (errno not guaranteed to be initialized)

    if (argc < 7) {
        printf("USAGE: ./mustang-engine [max threads] [hashtable capacity exponent] [cache capacity] [output file] [log file] [paths, ...]\n");
        printf("\tHINT: see mustang wrapper or invoke \"mustang -h\" for more details.\n");
        return 1;
    } 

    FILE* output_ptr = fopen(argv[5], "w");

    if (output_ptr == NULL) {
        LOG(LOG_ERR, "Failed to open file \"%s\" for writing to output (%s)\n", argv[4], strerror(errno));
        return 1;
    }

    // If stderr not being used for logging, redirect stdout and stderr to specified file (redirection is default behavior)
    if (strncmp(argv[6], "stderr", strlen("stderr")) != 0) {
        int log_fd = open(argv[6], O_WRONLY | O_CREAT | O_APPEND, 0644);

        if (dup2(log_fd, STDERR_FILENO) == -1) {
            printf("Failed to redirect stderr! (%s)\n", strerror(errno));
        }

        close(log_fd);
    }

    // subprocess.run() in frontend mandates passing an argument list of strings, so conversion back into numeric values required here
    char* invalid = NULL;
    size_t max_threads = (size_t) strtol(argv[1], &invalid, 10);

    if ((errno == EINVAL) || (*invalid != '\0')) {
        LOG(LOG_ERR, "Bad max threads argument \"%s\" received. Please specify a nonnegative integer (i.e. > 0), then try again.\n", argv[1]);
        fclose(output_ptr);
        return 1;
    }

    if (max_threads > 32768) {
        LOG(LOG_WARNING, "Using extremely large number of threads %zu. This may overwhelm system limits such as those set in /proc/sys/kernel/threads-max or /proc/sys/vm/max_map_count.\n", max_threads);
    }

    size_t queue_capacity;

    // Allow -1 as a sentinel for "unlimited" capacity
    if (strncmp(argv[2], "-1", strlen(argv[2])) == 0) {
        queue_capacity = SIZE_MAX;
        LOG(LOG_INFO, "Using SIZE_MAX as queue capacity (effectively unlimited).\n");
    } else {
        invalid = NULL;
        queue_capacity = (size_t) strtol(argv[2], &invalid, 10);

        if ((errno == EINVAL) || (*invalid != '\0')) {
            LOG(LOG_ERR, "Bad task queue capacity argument \"%s\" received. Please specify a nonnegative integer (i.e., > 0), then try again.\n", argv[2]);
            fclose(output_ptr);
            return 1;
        }
    }

    if (queue_capacity < max_threads) {
        LOG(LOG_WARNING, "Task queue capacity is less than maximum number of threads (i.e., thread pool size), which will limit concurrency by not taking full advantage of the thread pool.\n");
        LOG(LOG_WARNING, "Consider passing a task queue capacity argument that is greater than or equal to the maximum number of threads so that all threads have the chance to dequeue at least one task.\n");
    }

    invalid = NULL;
    long hashtable_capacity = strtol(argv[3], &invalid, 10);

    if ((hashtable_capacity < 2) || (((size_t) hashtable_capacity) > (HC_MAX)) || 
            (errno == EINVAL) || (*invalid != '\0')) {
        LOG(LOG_ERR, "Bad hashtable capacity argument \"%s\" received. Please specify a positive integer between (2**1) and (2**63), then try again.\n", argv[3]);
        fclose(output_ptr);
        return 1;
    }

    invalid = NULL;
    long fetched_id_cache_capacity = strtol(argv[4], &invalid, 10);

    if ((fetched_id_cache_capacity <= 0) || (errno == EINVAL) || (*invalid != '\0')) {
        LOG(LOG_ERR, "Bad cache capacity argument \"%s\" received. Please specify a nonnegative integer (i.e. > 0), then try again.\n", argv[4]);
        fclose(output_ptr);
        return 1;
    }

    id_cache_capacity = (size_t) fetched_id_cache_capacity; // cast afterwards to allow detecting negative values

    if (id_cache_capacity > 1024) {
        LOG(LOG_WARNING, "Provided cache capacity argument will result in large per-thread data structures, which may overwhelm the heap.\n");
    }


    hashtable* output_table = hashtable_init((size_t) hashtable_capacity);
    
    if ((output_table == NULL) || (errno == ENOMEM)) {
        LOG(LOG_ERR, "Failed to initialize hashtable (%s)\n", strerror(errno));
        fclose(output_ptr);
        return 1;
    }

    pthread_mutex_t ht_lock = PTHREAD_MUTEX_INITIALIZER;

    char* config_path = getenv("MARFS_CONFIG_PATH");

    if (config_path == NULL) {
        LOG(LOG_ERR, "MARFS_CONFIG_PATH not set in environment--please set and try again.\n");
        return 1;
    }

    task_queue* queue = task_queue_init((size_t) queue_capacity);

    if (queue == NULL) {
        LOG(LOG_ERR, "Failed to initialize task queue! (%s)\n", strerror(errno));
        return 1;
    }

    pthread_mutex_t erasure_lock = PTHREAD_MUTEX_INITIALIZER;

    marfs_config* parent_config = config_init(config_path, &erasure_lock);    
    marfs_position parent_position = { .ns = NULL, .depth = 0, .ctxt = NULL };

    if (config_establishposition(&parent_position, parent_config)) {
        LOG(LOG_ERR, "Failed to establish marfs_position!\n");
        return 1;
    }

    if (config_fortifyposition(&parent_position)) {
        LOG(LOG_ERR, "Failed to fortify position with MDAL_CTXT!\n");
        config_abandonposition(&parent_position);
        return 1;
    }

    pthread_attr_t pooled_attr_template;
    pthread_attr_t* attr_ptr = &pooled_attr_template;
    
    if (pthread_attr_init(attr_ptr)) {
        LOG(LOG_ERR, "Failed to initialize attributes for pooled threads!\n");
        LOG(LOG_WARNING, "This means that stack size cannot be reduced from the default, which may overwhelm system resources unexpectedly.\n");
        attr_ptr = NULL;
    }

    if (pthread_attr_setstacksize(&pooled_attr_template, 2 * PTHREAD_STACK_MIN)) {
        LOG(LOG_ERR, "Failed to set stack size for pooled threads! (%s)\n", strerror(errno));
        LOG(LOG_WARNING, "This means that threads will proceed with the default stack size, which is unnecessarily large for this application and which may overwhelm system resources unexpectedly.\n");
        attr_ptr = NULL;
    }

    // Malloc used here since initialization not important (pthread_ts will be changed with pthread_create())
    pthread_t* worker_pool = (pthread_t*) malloc(max_threads * sizeof(pthread_t));

    if (worker_pool == NULL) {
        LOG(LOG_ERR, "Failed to allocate memory for worker pool! (%s)\n", strerror(errno));
        return 1;
    }

    for (size_t i = 0; i < max_threads; i += 1) {
        int create_errorcode = pthread_create(&(worker_pool[i]), attr_ptr, &thread_launcher, queue);
        if (create_errorcode) {
            LOG(LOG_ERR, "Failed to create thread! (%s)\n", strerror(create_errorcode));
            return 1;
        }
    }

    for (int index = 7; index < argc; index += 1) {
        LOG(LOG_INFO, "Processing arg \"%s\"\n", argv[index]);

        struct stat arg_statbuf;

        int statcode = stat(argv[index], &arg_statbuf);

        if (statcode) {
            LOG(LOG_ERR, "Failed to stat path arg \"%s\" (%s)--skipping to next\n", argv[index], strerror(errno));
            continue;
        }

        if ((arg_statbuf.st_mode & S_IFMT) != S_IFDIR) {
            LOG(LOG_WARNING, "Path arg \"%s\" does not target a directory--skipping to next\n", argv[index]);
            continue;
        }

        marfs_position* new_task_position = calloc(1, sizeof(marfs_position));

        if (config_duplicateposition(&parent_position, new_task_position)) {
            LOG(LOG_ERR, "Failed to duplicate parent position to new task!\n");
        }

        char* next_basepath = strdup(argv[index]);

        int new_task_depth = config_traverse(parent_config, new_task_position, &next_basepath, 0);

        if (new_task_depth < 0) {
            LOG(LOG_ERR, "Failed to traverse (got depth: %d)\n", new_task_depth);
            free(next_basepath);
            config_abandonposition(new_task_position);
            continue;
        }

        if (config_fortifyposition(new_task_position)) {
            LOG(LOG_ERR, "Failed to fortify new_task position after new_task traverse!\n");
        }

        MDAL_DHANDLE task_dirhandle;

        MDAL task_mdal = new_task_position->ns->prepo->metascheme.mdal;

        if (new_task_depth != 0) {
            task_dirhandle = task_mdal->opendir(new_task_position->ctxt, next_basepath);

            if (task_dirhandle == NULL) {
                LOG(LOG_ERR, "Failed to open target directory \"%s\" (%s)\n", next_basepath, strerror(errno));
            }

            if (task_mdal->chdir(new_task_position->ctxt, task_dirhandle)) {
                LOG(LOG_ERR, "Failed to chdir into target directory \"%s\" (%s)\n", next_basepath, strerror(errno));
            }
        }

        new_task_position->depth = new_task_depth; 

        mustang_task* top_task;

        switch (new_task_depth) {
            case 0:
                top_task = task_init(parent_config, new_task_position, output_table, &ht_lock, queue, &traverse_ns);
                task_enqueue(queue, top_task);
                LOG(LOG_DEBUG, "Created top-level namespace traversal task at basepath: \"%s\"\n", next_basepath);
                break;
            default:
                top_task = task_init(parent_config, new_task_position, output_table, &ht_lock, queue, &traverse_dir);
                task_enqueue(queue, top_task);
                LOG(LOG_DEBUG, "Created top-level directory traversal task at basepath: \"%s\"\n", next_basepath);
                break;
        }

        free(next_basepath);
    }

    pthread_mutex_lock(queue->lock);
    while ((queue->todos > 0) || (queue->size > 0)) {
        pthread_cond_wait(queue->manager_cv, queue->lock);
    }
    pthread_mutex_unlock(queue->lock); 

    // Once there are no tasks left in the queue to do, send workers sentinel (all-NULL) tasks so that they know to exit.
    for (size_t i = 0; i < max_threads; i += 1) {
        mustang_task* sentinel = task_init(NULL, NULL, NULL, NULL, NULL, NULL);
        task_enqueue(queue, sentinel);
    }

    // Threads should have exited by this point, so join them.
    for (size_t i = 0; i < max_threads; i += 1) {
        if (pthread_join(worker_pool[i], NULL)) {
            LOG(LOG_ERR, "Failed to join thread %0lx! (%s)\n", worker_pool[i], strerror(errno));
        }
    }

    if (task_queue_destroy(queue)) {
        LOG(LOG_ERR, "Failed to destroy task queue! (%s)\n", strerror(errno));
        LOG(LOG_WARNING, "This is a critical application error meaning thread-safety measures have failed. You are strongly advised to disregard the output of this run and attempt another invocation.\n");
    }

    free(worker_pool);

    pthread_mutex_lock(&ht_lock);
    // hashtable_dump() returns the result of fclose(), which can set errno
    if (hashtable_dump(output_table, output_ptr)) {
        LOG(LOG_WARNING, "Failed to close hashtable output file pointer! (%s)\n", strerror(errno));
    }
    pthread_mutex_unlock(&ht_lock);

    // If attr could be initialized (and was therefore kept non-NULL), then destroy the attributes
    if (attr_ptr != NULL) {
        pthread_attr_destroy(attr_ptr);
    }

    // Clean up hashtable and associated lock state
    hashtable_destroy(output_table);
    pthread_mutex_destroy(&ht_lock);

    if (config_abandonposition(&parent_position)) {
        LOG(LOG_WARNING, "Failed to abandon parent position!\n");
    }

    if (config_term(parent_config)) {
        LOG(LOG_WARNING, "Failed to terminate parent config!\n");
    }

    pthread_mutex_destroy(&erasure_lock);

    return 0;
}
