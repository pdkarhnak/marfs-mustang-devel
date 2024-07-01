#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <marfs.h>
#include <config/config.h>
#include <datastream/datastream.h>

#ifdef DEBUG
#include <assert.h>
#define ID_MASK 0xFFFFFFFF
#define SHORT_SELFID() (pthread_self() & ID_MASK)
#define SHORT_ID(id) (id & ID_MASK)
#endif

#include "hashtable.h"
#include "mustang_threading.h"
#include "pthread_vector.h"
#include "retcode_ll.h"

#define WARN(message, errorcode) \
    printf("WARNING: %s (%s)\n", message, strerror(errorcode));

#define WARN_MSG(message) \
    printf("[thread %0lx -- parent] WARNING: %s\n", pthread_self(), message)

#define ERR(message, errorcode) \
    printf("ERROR: %s (%s)", message, strerror(errorcode));

#define ERR_MSG(message) \
    printf("[thread %0lx -- parent] ERROR: %s\n", pthread_self(), message)

extern void* thread_main(void* args);

int main(int argc, char** argv) {

    if (argc < 3) {
        printf("USAGE: ./mustang-engine [output file] [log file] [max threads] [paths, ...]\n");
        printf("\tHINT: see mustang wrapper or invoke \"mustang -h\" for more details.\n");
        return 1;
    }

    FILE* output_ptr = fopen(argv[1], "w");
    FILE* logfile_ptr = fopen(argv[2], "w");

    if (output_ptr == NULL) {
        printf("ERROR: failed to open file \"%s\"\n", argv[1]);
        return 1;
    }

    if (logfile_ptr == NULL) {
        printf("ERROR: failed to open file \"%s\"\n", argv[2]);
    }

    hashtable* output_table = hashtable_init();
    
    if ((output_table == NULL) || (errno == ENOMEM)) {
        ERR("No memory left--failed to allocate hashtable.", errno);
        printf("\n");
        return 1;
    }

    pthread_mutex_t ht_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t logfile_lock = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG
    pthread_mutex_t out_lock;
    pthread_mutex_init(&out_lock, NULL);
#endif

    pthread_vector* top_threads = pthread_vector_init(DEFAULT_CAPACITY);

    char* config_path = getenv("MARFS_CONFIG_PATH");

    if (config_path == NULL) {
        ERR_MSG("MARFS_CONFIG_PATH not set in environment--please set and try again.");
        return 1;
    }

    pthread_mutex_t erasure_lock = PTHREAD_MUTEX_INITIALIZER;
    marfs_config* parent_config = config_init(config_path, &erasure_lock);    

    marfs_position parent_position = { .ns = NULL, .depth = 0, .ctxt = NULL };

    if (config_establishposition(&parent_position, parent_config)) {
        ERR_MSG("Failed to establish marfs_position!");
        return 1;
    }

    if (config_fortifyposition(&parent_position)) {
        ERR_MSG("Failed to fortify position with MDAL_CTXT!");
        config_abandonposition(&parent_position);
        return 1;
    }

    MDAL parent_mdal = parent_position.ns->prepo->metascheme.mdal;
    
    for (int index = 3; index < argc; index += 1) {
#ifdef DEBUG
        printf("NOTE: processing arg \"%s\"\n", argv[index]);
#endif

        marfs_position* child_position = calloc(1, sizeof(marfs_position));

        if (config_duplicateposition(&parent_position, child_position)) {
            ERR_MSG("Failed to duplicate parent position to child!");
        }

        char* next_basepath = strndup(argv[index], strlen(argv[index]));

        int child_depth = config_traverse(parent_config, child_position, &next_basepath, 0);

        if (config_fortifyposition(child_position)) {
            ERR_MSG("Failed to fortify child position after child traverse!");
        }

        if (child_depth < 0) {
            ERR_MSG("Failed to traverse! ");
            printf("(got depth %d)\n", child_depth);
            free(next_basepath);
            config_abandonposition(child_position);
            continue;
        }

        MDAL_DHANDLE child_dirhandle;

        MDAL current_child_mdal = child_position->ns->prepo->metascheme.mdal;

        if (child_depth == 0) {
            child_dirhandle = parent_mdal->opendirnamespace(child_position->ctxt, argv[index]);
        } else {
            child_dirhandle = current_child_mdal->opendir(child_position->ctxt, argv[index]);
        }

        if (child_dirhandle == NULL) {
            ERR("Failed to open target directory/namespace", errno);
            printf(" [target: %s]\n", argv[index]);
        }

        if (current_child_mdal->chdir(child_position->ctxt, child_dirhandle)) {
            ERR("Failed to chdir into target directory", errno);
            printf(" [target: %s]\n", argv[index]);
        }


        child_position->depth = child_depth;

        thread_args* topdir_args = threadarg_init(parent_config, child_position, output_table, &ht_lock, next_basepath, logfile_ptr, &logfile_lock);

#ifdef DEBUG
        topdir_args->stdout_lock = &out_lock;        
#endif

        pthread_t next_id;
        pthread_create(&next_id, NULL, &thread_main, (void*) topdir_args);
        pthread_vector_append(top_threads, next_id);

#ifdef DEBUG
        pthread_mutex_lock(&out_lock);
        printf("[thread %0lx -- parent]: created top thread with ID: %0lx and target basepath: '%s'\n", SHORT_SELFID(), SHORT_ID(next_id), next_basepath);
        pthread_mutex_unlock(&out_lock);
#endif

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
#ifdef DEBUG
            pthread_mutex_lock(&out_lock);
            printf("[thread %0lx -- parent]: ERROR: failed to join child thread with ID: %0lx\n", SHORT_SELFID(), SHORT_ID(join_id));
            pthread_mutex_unlock(&out_lock);
#endif
            continue;
        }

        if (joined_ll == NULL) {
#ifdef DEBUG
            pthread_mutex_lock(&out_lock);
            printf("[thread %0lx -- parent]: ERROR: child thread (ID %0lx) returned NULL (was unable to allocate memory).\n", SHORT_SELFID(), SHORT_ID(join_id));
            pthread_mutex_unlock(&out_lock);
            continue;
#endif
        }

        parent_ll = retcode_ll_concat(parent_ll, joined_ll);

        if (parent_ll->size >= RC_LL_LEN_MAX) {
            retcode_ll_flush(parent_ll, logfile_ptr, &logfile_lock);    
        }

    }

    retcode_ll_flush(parent_ll, logfile_ptr, &logfile_lock);
    retcode_ll_destroy(parent_ll);

#ifdef DEBUG
    pthread_mutex_destroy(&out_lock);
#endif

    pthread_mutex_lock(&ht_lock);
    hashtable_dump(output_table, output_ptr);
    pthread_mutex_unlock(&ht_lock);

    if (fclose(output_ptr)) {
        WARN("Failed to close output file pointer.", errno);
    }

    if (fclose(logfile_ptr)) {
        WARN("Failed to close log file pointer.", errno);
    }

    pthread_vector_destroy(top_threads);
    hashtable_destroy(output_table);
    pthread_mutex_destroy(&ht_lock);

    if (config_abandonposition(&parent_position)) {
        WARN_MSG("Failed to abandon parent position!");
    }

    if (config_term(parent_config)) {
        WARN_MSG("Failed to terminate marfs_config!");
    }

    pthread_mutex_destroy(&erasure_lock);

    return 0;

}
