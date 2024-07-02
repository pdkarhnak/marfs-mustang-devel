#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <libgen.h>
#include <marfs.h>
#include <config/config.h>
#include <datastream/datastream.h>

#define LOG_PREFIX "mustang_engine"
#include <logging/logging.h>

#ifdef DEBUG
#include <assert.h>
#define ID_MASK 0xFFFFFFFF
#define SHORT_ID(id) (id & ID_MASK)
#endif

#include "hashtable.h"
#include "mustang_threading.h"
#include "pthread_vector.h"
#include "retcode_ll.h"

extern void* thread_main(void* args);

int main(int argc, char** argv) {

    if (argc < 3) {
        printf("USAGE: ./mustang-engine [output file] [log file] [max threads] [paths, ...]\n");
        printf("\tHINT: see mustang wrapper or invoke \"mustang -h\" for more details.\n");
        return 1;
    }

    FILE* output_ptr = fopen(argv[1], "w");

    if (output_ptr == NULL) {
        LOG(LOG_ERR, "Failed to open file \"%s\" for writing to output (%s)\n", argv[1], strerror(errno));
        return 1;
    }

    // If stdout not being used for logging, redirect stdout and stderr to specified file
    if (strncmp(argv[2], "stdout", strlen("stdout")) != 0) {
        int log_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (dup2(log_fd, STDOUT_FILENO) == -1) {
            LOG(LOG_ERR, "Failed to redirect stdout to log file fd (%s)\n", strerror(errno));
            fclose(output_ptr);
            close(log_fd);
            return 1;
        }
        
        if (dup2(log_fd, STDERR_FILENO) == -1) {
            LOG(LOG_ERR, "Failed to redirect stderr to logfile fd (%s)\n", strerror(errno));
            fclose(output_ptr);
            close(log_fd);
            return 1;
        }

        close(log_fd); // stdout and stderr have been redirected, so no need for the new fd
    }

    hashtable* output_table = hashtable_init();
    
    if ((output_table == NULL) || (errno == ENOMEM)) {
        LOG(LOG_ERR, "Failed to initialize hashtable (%s)\n", strerror(errno));
        return 1;
    }

    pthread_mutex_t ht_lock = PTHREAD_MUTEX_INITIALIZER;

    // For debugging
    pthread_mutex_t logging_lock;
    pthread_mutex_init(&logging_lock, NULL);

    pthread_vector* top_threads = pthread_vector_init(DEFAULT_CAPACITY);

    char* config_path = getenv("MARFS_CONFIG_PATH");

    if (config_path == NULL) {
        LOG(LOG_ERR, "MARFS_CONFIG_PATH not set in environment--please set and try again.\n");
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
 
    for (int index = 3; index < argc; index += 1) {
        pthread_mutex_lock(&logging_lock);
        LOG(LOG_INFO, "Processing arg \"%s\"\n", argv[index]);
        pthread_mutex_unlock(&logging_lock);

        marfs_position* child_position = calloc(1, sizeof(marfs_position));

        if (config_duplicateposition(&parent_position, child_position)) {
            LOG(LOG_ERR, "Failed to duplicate parent position to child!\n");
        }

        char* next_basepath = strdup(argv[index]);
        char* basename;

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
                pthread_mutex_lock(&logging_lock);
                LOG(LOG_ERR, "Failed to open target directory \"%s\" (%s)\n", next_basepath, strerror(errno));
                pthread_mutex_unlock(&logging_lock);
            }

            if (current_child_mdal->chdir(child_position->ctxt, child_dirhandle)) {
                pthread_mutex_lock(&logging_lock);
                LOG(LOG_ERR, "Failed to chdir into target directory \"%s\" (%s)\n", next_basepath, strerror(errno));
                pthread_mutex_unlock(&logging_lock);
            }
        }

        child_position->depth = child_depth;

        thread_args* topdir_args = threadarg_init(parent_config, child_position, output_table, &ht_lock, next_basepath, &logging_lock);

        pthread_t next_id;
        pthread_create(&next_id, NULL, &thread_main, (void*) topdir_args);
        pthread_vector_append(top_threads, next_id);

        pthread_mutex_lock(&logging_lock);
        LOG(LOG_INFO, "Created top-level thread with ID: %0lx and basepath \"%s\"\n", SHORT_ID(next_id), next_basepath);
        pthread_mutex_unlock(&logging_lock);

    }
 
    pthread_t join_id;

    retcode_ll* parent_ll = retcode_ll_init();

    retcode_ll* joined_ll;

    for (int index = 0; index < top_threads->size; index += 1) {
        int findcode = at_index(top_threads, index, &join_id);

        if (findcode == -1) {
            // TODO: log
            continue;
        }

        int joincode = pthread_join(join_id, (void**) &joined_ll);

        if (joincode != 0) {
            pthread_mutex_lock(&logging_lock);
            LOG(LOG_ERR, "Failed to join child thread (child ID: %0lx)\n", SHORT_ID(join_id));
            pthread_mutex_unlock(&logging_lock);
            continue;
        }

        if (joined_ll == NULL) {
            pthread_mutex_lock(&logging_lock);
            LOG(LOG_ERR, "Child thread (ID: %0lx) returned NULL -- was unable to allocte memory.\n", SHORT_ID(join_id));
            pthread_mutex_unlock(&logging_lock);
            continue;
        }

        parent_ll = retcode_ll_concat(parent_ll, joined_ll);

        if (parent_ll->size >= RC_LL_LEN_MAX) {
            retcode_ll_flush(parent_ll, &logging_lock);    
        }

    }

    retcode_ll_flush(parent_ll, &logging_lock);
    retcode_ll_destroy(parent_ll);

    pthread_mutex_destroy(&logging_lock);

    pthread_mutex_lock(&ht_lock);
    hashtable_dump(output_table, output_ptr);
    pthread_mutex_unlock(&ht_lock);

    if (fclose(output_ptr)) {
        LOG(LOG_WARNING, "Failed to close output file pointer! (%s)\n", strerror(errno));
    }

    pthread_vector_destroy(top_threads);
    hashtable_destroy(output_table);
    pthread_mutex_destroy(&ht_lock);

    if (config_abandonposition(&parent_position)) {
        LOG(LOG_WARNING, "Failed to abandon parent position!\n");
    }

    if (config_term(parent_config)) {
        LOG(LOG_WARNING, "Failed to terminate marfs_config!\n");
    }

    pthread_mutex_destroy(&erasure_lock);

    return 0;

}
