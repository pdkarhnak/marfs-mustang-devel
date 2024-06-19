#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef DEBUG
#include <assert.h>
#define SHORT_ID() (pthread_self() & 0xFFFF)
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
        printf("USAGE: ./mustang-engine [output file] [max threads] [paths, ...]\n");
        printf("\tHINT: see mustang wrapper or invoke \"mustang -h\" for more details.\n");
        return 1;
    }

    char* output_filepath = argv[1];
    FILE* output_ptr = fopen(output_filepath, "w");

    if (output_ptr == NULL) {
        printf("ERROR: failed to open file \"%s\"\n", argv[1]);
        return 1;
    }

    hashtable* output_table = hashtable_init();
    pthread_mutex_t* ht_lock = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(ht_lock, NULL);

    const size_t max_threads = ((size_t) atol(argv[2]));

    threadcount_verifier* verifier = verifier_init(max_threads);

#ifdef DEBUG
    pthread_mutex_t* out_lock = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(out_lock, NULL);
#endif

    pthread_vector* pt_vec = pthread_vector_init(max_threads);

    if ((output_table == NULL) || (errno == ENOMEM)) {
        ERR("No memory left--failed to allocate hashtable.", errno)
        return 1;
    }

    thread_args* top_args = (thread_args*) calloc(1, sizeof(thread_args));

    if ((top_args == NULL) || (errno == ENOMEM)) {
        ERR("No memory left--failed to allocate thread_args struct.", errno);
        return 1;
    }

    top_args->tc_verifier = verifier;
    top_args->pt_vector = pt_vec;
    top_args->hashtable = output_table;
    top_args->hashtable_lock = ht_lock;
    top_args->basepath = strdup(argv[3]);
    top_args->cwd_fd = open(argv[3], O_RDONLY | O_DIRECTORY);

#ifdef DEBUG
    if (top_args->cwd_fd == -1) {
        pthread_mutex_lock(out_lock);
        printf("ERROR: open() failed! (%s)\n", strerror(errno));
        pthread_mutex_unlock(out_lock);
        return 1;
    }
#endif

#ifdef DEBUG
    top_args->stdout_lock = out_lock;
#endif

    pthread_t top_thread;
    pthread_create(&top_thread, NULL, &thread_routine, (void*) top_args);

#ifdef DEBUG
    pthread_mutex_lock(out_lock);
    printf("[thread %0lx -- parent]: created top thread with ID: %0lx\n", SHORT_ID(), (top_thread & 0xFFFF));
    pthread_mutex_unlock(out_lock);
#endif

    pthread_vector_appendset(pt_vec, &top_thread, 1);
    
    size_t* perthread_retval;

    for (int index = 0; index < (pt_vec->size); index += 1) {
        pthread_vector_pollthread(pt_vec, (void**) &perthread_retval, index);
#ifdef DEBUG
        pthread_mutex_lock(out_lock);
        printf("[thread %0lx -- parent]: Thread at index %d returned: %zu\n", SHORT_ID(), index, ((size_t) perthread_retval));
        pthread_mutex_unlock(out_lock);
#endif
    }

#ifdef DEBUG
    pthread_mutex_destroy(out_lock);
    free(out_lock);
    out_lock = NULL;
#endif

    hashtable_dump(output_table, output_ptr);
    fclose(output_ptr);

    verifier_destroy(verifier);
    pthread_vector_destroy(pt_vec);
    hashtable_destroy(output_table);
    pthread_mutex_destroy(ht_lock);
    free(ht_lock);

    return 0;

}
