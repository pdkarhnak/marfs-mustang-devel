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

#define WARN(message, errorcode) \
    printf("WARN: %s (%s)\n", message, strerror(errorcode));

#define ERR(message, errorcode) \
    printf("ERROR: %s (%s)\n", message, strerror(errorcode));

extern void* thread_routine(void* args);

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

        thread_args* topdir_args = threadarg_init(output_table, &ht_lock, next_basepath, next_cwd_fd);

#ifdef DEBUG
        topdir_args->stdout_lock = &out_lock;        
#endif

        pthread_t next_id;
        pthread_create(&next_id, NULL, &thread_routine, (void*) topdir_args);
        pthread_vector_append(top_threads, next_id);

#ifdef DEBUG
        pthread_mutex_lock(&out_lock);
        printf("[thread %0lx -- parent]: created top thread with ID: %0lx and target basepath: '%s'\n", SHORT_SELFID(), SHORT_ID(top_pthread_set[index - 3]), next_basepath);
        pthread_mutex_unlock(&out_lock);
#endif

    }
 
    // TODO: convert to new retcode_ll scheme
    size_t* perthread_retval;

    for (int index = 0; index < (pt_vec->size); index += 1) {
        pthread_vector_pollthread(pt_vec, (void**) &perthread_retval, index);
#ifdef DEBUG
        pthread_mutex_lock(&out_lock);
        printf("[thread %0lx -- parent]: Thread at index %d returned: %zu\n", SHORT_SELFID(), index, ((size_t) perthread_retval));
        pthread_mutex_unlock(&out_lock);
#endif
    }

#ifdef DEBUG
    pthread_mutex_destroy(&out_lock);
#endif

    hashtable_dump(output_table, output_ptr);
    fclose(output_ptr);
    fclose(logfile_ptr);

    pthread_vector_destroy(top_threads);
    hashtable_destroy(output_table);
    pthread_mutex_destroy(&ht_lock);

    return 0;

}
