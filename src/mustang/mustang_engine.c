#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <marfs.h>
#include <config/config.h>
#include <datastream/datastream.h>

#ifdef DEBUG_MUSTANG
#define DEBUG DEBUG_MUSTANG
#elif (defined DEBUG_ALL)
#define DEBUG DEBUG_ALL
#endif

#include "mustang_logging.h"
#define LOG_PREFIX "mustang_engine"
#include <logging/logging.h>

#include "hashtable.h"
#include "mustang_threading.h"
#include "retcode_ll.h"

extern void* thread_main(void* args);
size_t id_cache_capacity;

int main(int argc, char** argv) {

    // TODO: update arg indices.
    // Max threads is argv[1] now, so shift everything else "back" in index by one

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

    // If stdout not being used for logging, redirect stdout and stderr to specified file
    if (strncmp(argv[5], "stderr", strlen("stderr")) != 0) {
        int log_fd = open(argv[5], O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (dup2(log_fd, STDERR_FILENO) == -1) {
            LOG(LOG_ERR, "Failed to redirect stderr to log file fd! (%s)\n", strerror(errno));
            fclose(output_ptr);
            close(log_fd);
            return 1;
        }

        close(log_fd); // stdout and stderr have been redirected, so no need for the new fd
    }


    char* invalid = NULL;
    size_t capacity_power = (size_t) strtol(argv[2], &invalid, 10);

    if ((capacity_power <= 0) || (capacity_power >= 64) || 
            (errno == EINVAL) || (*invalid != '\0')) {
        LOG(LOG_ERR, "Bad hashtable capacity argument \"%s\" received. Please specify a positive integer between 1 and 64, then try again.\n", argv[2]);
        fclose(output_ptr);
        return 1;
    }

    size_t computed_capacity = 1;
    computed_capacity <<= capacity_power;

    if (capacity_power < 5) {
        LOG(LOG_WARNING, "Provided hashtable capacity argument \"%s\" will result in very small capacity %zu\n", argv[2], computed_capacity);
    } else if (capacity_power >= 33) {
        LOG(LOG_WARNING, "Provided hashtable capacity argument \"%s\" will result in very large capacity %zu\n", argv[2], computed_capacity);
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

    hashtable* output_table = hashtable_init(computed_capacity);
    
    if ((output_table == NULL) || (errno == ENOMEM)) {
        LOG(LOG_ERR, "Failed to initialize hashtable (%s)\n", strerror(errno));
        fclose(output_ptr);
        return 1;
    }

    capacity_monitor_t* threads_capacity_monitor = monitor_init();

    pthread_rwlock_t ht_lock = PTHREAD_RWLOCK_INITIALIZER;

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
 
    ssize_t successful_spawns = 0;

    for (int index = 6; index < argc; index += 1) {
#if (DEBUG == 1)
        LOG(LOG_INFO, "Processing arg \"%s\"\n", argv[index]);
#endif

        struct stat arg_statbuf;

        int statcode = stat(argv[index], &arg_statbuf);

        if (statcode) {
            LOG(LOG_ERR, "Failed to stat path arg \"%s\" (%s)--skipping to next\n", argv[index], strerror(errno));
            continue;
        }

        if ((arg_statbuf.st_mode & S_IFMT) != S_IFDIR) {
#if (DEBUG <= 2)
            LOG(LOG_WARNING, "Path arg \"%s\" does not target a directory--skipping to next\n", argv[index]);
#endif
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

        thread_args* topdir_args = threadarg_init(parent_config, child_position, output_table, &ht_lock, next_basepath);

        pthread_t next_id;
        
        if (pthread_create(&next_id, NULL, &thread_main, (void*) topdir_args) == 0) {
            successful_spawns += 1; // TODO: unite error-handling code into separate branch of this if statement
        }

        LOG(LOG_INFO, "Created top-level thread with ID: %0lx and basepath \"%s\"\n", SHORT_ID(next_id), next_basepath);
    }

    countdown_monitor_windup(/* TODO: put monitor pointer here after allocating */, successful_spawns);
 
    pthread_rwlock_rdlock(&ht_lock);
    hashtable_dump(output_table, output_ptr);
    pthread_rwlock_unlock(&ht_lock);

    if (fclose(output_ptr)) {
        LOG(LOG_WARNING, "Failed to close output file pointer! (%s)\n", strerror(errno));
    }

    hashtable_destroy(output_table);

    if (config_abandonposition(&parent_position)) {
        LOG(LOG_WARNING, "Failed to abandon parent position!\n");
    }

    if (config_term(parent_config)) {
        LOG(LOG_WARNING, "Failed to terminate marfs_config!\n");
    }

    pthread_mutex_destroy(&erasure_lock);

    return 0;

}
