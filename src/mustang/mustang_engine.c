#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
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

extern void* thread_launcher(void* args);

size_t id_cache_capacity;

int main(int argc, char** argv) {

    errno = 0; // to guarantee an initially successful context and avoid "false positive" errno settings (errno not guaranteed to be initialized)

    if (argc < 6) {
        printf("USAGE: ./mustang-engine [max threads] [hashtable capacity exponent] [cache capacity] [output file] [log file] [paths, ...]\n");
        printf("\tHINT: see mustang wrapper or invoke \"mustang -h\" for more details.\n");
        return 1;
    } 

    FILE* output_ptr = fopen(argv[4], "w");

    if (output_ptr == NULL) {
        LOG(LOG_ERR, "Failed to open file \"%s\" for writing to output (%s)\n", argv[4], strerror(errno));
        return 1;
    }

    // If stderr not being used for logging, redirect stdout and stderr to specified file (redirection is default behavior)
    if (strncmp(argv[5], "stderr", strlen("stderr")) != 0) {
        int log_fd = open(argv[5], O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (dup2(log_fd, STDERR_FILENO) == -1) {
            printf("Failed to redirect stderr! (%s)\n", strerror(errno));
        }

        close(log_fd);
    }

    // subprocess.run() in frontend mandates passing an argument list of strings, so conversion back into numeric values required here
    char* invalid = NULL;
    size_t hashtable_capacity = (size_t) strtol(argv[2], &invalid, 10);

    if ((hashtable_capacity < 2) || (hashtable_capacity > (1 << 63)) || 
            (errno == EINVAL) || (*invalid != '\0')) {
        LOG(LOG_ERR, "Bad hashtable capacity argument \"%s\" received. Please specify a positive integer between (2**1) and (2**63), then try again.\n", argv[2]);
        fclose(output_ptr);
        return 1;
    }

    invalid = NULL;
    id_cache_capacity = (size_t) strtol(argv[3], &invalid, 10);

    if ((id_cache_capacity <= 0) || (errno == EINVAL) || (*invalid != '\0')) {
        LOG(LOG_ERR, "Bad cache capacity argument \"%s\" received. Please specify a nonnegative integer (i.e. > 0), then try again.\n", argv[3]);
        fclose(output_ptr);
        return 1;
    }

    if (id_cache_capacity > 1024) {
        LOG(LOG_WARNING, "Provided cache capacity argument will result in large per-thread data structures, which may overwhelm the heap.\n");
    }

    invalid = NULL;
    size_t max_threads = (size_t) strtol(argv[1], &invalid, 10);

    if ((max_threads <= 0) || (errno == EINVAL) || (*invalid != '\0')) {
        LOG(LOG_ERR, "Bad max threads argument \"%s\" received. Please specify a nonnegative integer (i.e. > 0), then try again.\n", argv[3]);
        fclose(output_ptr);
        return 1;
    }

    hashtable* output_table = hashtable_init(hashtable_capacity);
    
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

    task_queue* queue = task_queue_init(/* TODO: add reference to queue capacity argument */);

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

    pthread_attr_t child_attr_template;
    pthread_attr_t* attr_ptr = &child_attr_template;
    
    if (pthread_attr_init(attr_ptr)) {
        LOG(LOG_ERR, "Failed to initialize attributes for child threads!\n");
        LOG(LOG_WARNING, "This means that stack size cannot be reduced from the default, which may overwhelm system resources unexpectedly.\n");
        attr_ptr = NULL;
    }

    if (pthread_attr_setstacksize(&child_attr_template, 2 * PTHREAD_STACK_MIN)) {
        LOG(LOG_ERR, "Failed to set stack size for child threads! (%s)\n", strerror(errno));
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

    // This should be the code that creates *tasks*, not *threads*
    for (int index = 6; index < argc; index += 1) {
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

        marfs_position* child_position = calloc(1, sizeof(marfs_position));

        if (config_duplicateposition(&parent_position, child_position)) {
            LOG(LOG_ERR, "Failed to duplicate parent position to child!\n");
        }

        char* next_basepath = strdup(argv[index]);

        int child_depth = config_traverse(parent_config, child_position, &next_basepath, 0);

        if (child_depth < 0) {
            LOG(LOG_ERR, "Failed to traverse (got depth: %d)\n", child_depth);
            free(next_basepath);
            config_abandonposition(child_position);
            continue;
        }

        if (config_fortifyposition(child_position)) {
            LOG(LOG_ERR, "Failed to fortify child position after child traverse!\n");
        }

        MDAL_DHANDLE child_dirhandle;

        MDAL current_child_mdal = child_position->ns->prepo->metascheme.mdal;

        if (child_depth != 0) {
            child_dirhandle = current_child_mdal->opendir(child_position->ctxt, next_basepath);

            if (child_dirhandle == NULL) {
                LOG(LOG_ERR, "Failed to open target directory \"%s\" (%s)\n", next_basepath, strerror(errno));
            }

            if (current_child_mdal->chdir(child_position->ctxt, child_dirhandle)) {
                LOG(LOG_ERR, "Failed to chdir into target directory \"%s\" (%s)\n", next_basepath, strerror(errno));
            }
        }

        child_position->depth = child_depth; 
        // TODO: put in switch-case statement to create task according to namespace traversal or directory traversal

    }

    pthread_mutex_lock(queue->lock);
    while ((queue->todos > 0) && (queue->size > 0)) {
        pthread_cond_wait(queue->manager_cv);
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
    hashtable_dump(output_table, output_ptr);
    pthread_mutex_unlock(&ht_lock);

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
