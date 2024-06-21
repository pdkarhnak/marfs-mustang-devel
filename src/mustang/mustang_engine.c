#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <marfs.h>

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
    printf("WARN: %s (%s)\n", message, strerror(errorcode));

#define ERR(message, errorcode) \
    printf("ERROR: %s (%s)\n", message, strerror(errorcode));

extern void* thread_main(void* args);

int main(int argc, char** argv) {

    if (argc < 4) {
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
        ERR("No memory left--failed to allocate hashtable.", errno)
        return 1;
    }

    pthread_mutex_t ht_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t logfile_lock = PTHREAD_MUTEX_INITIALIZER;

    const size_t max_threads = ((size_t) atol(argv[3]));

#ifdef DEBUG
    pthread_mutex_t out_lock;
    pthread_mutex_init(&out_lock, NULL);
#endif

    pthread_vector* top_threads = pthread_vector_init(DEFAULT_CAPACITY);

    for (int index = 4; index < argc; index += 1) {

        int next_cwd_fd = open(argv[index], O_RDONLY | O_DIRECTORY);
        char* next_basepath = strndup(argv[index], strlen(argv[index]));

#ifdef DEBUG
        if (next_cwd_fd == -1) {
            pthread_mutex_lock(&out_lock);
            printf("ERROR: open() failed! (%s)\n", strerror(errno));
            pthread_mutex_unlock(&out_lock);
            return 1;
        }
#endif

        thread_args* topdir_args = threadarg_init(output_table, &ht_lock, next_basepath, next_cwd_fd, logfile_ptr, &logfile_lock);

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
 
    // TODO: convert to new retcode_ll scheme
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

#ifdef DEBUG
        assert(joined_ll->head != NULL);
        assert(joined_ll->tail != NULL);
#endif

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

    fclose(output_ptr);
    fclose(logfile_ptr);

    pthread_vector_destroy(top_threads);
    hashtable_destroy(output_table);
    pthread_mutex_destroy(&ht_lock);

    return 0;

}
