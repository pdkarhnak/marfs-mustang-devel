#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "hashtable/hashtable.h"
#include "pthread_vector.h"

int main(int argc, char** argv) {

    size_t max_threads = argv[1];

    int arg_index = 3;

    int pathcount = 0;
    int nscount = 0;
    // "[namespaces:]"

    // pretend like you\'re doing an unconditional traversal from root every time--it\'ll simplify things

    /*
    for ( ; arg_index < argc; arg_index += 1) {
        if (strncmp(argv[arg_index], "[namespaces:]", strlen(argv[arg_index])) == 0) {
            arg_index += 1;
            break;
        }

        pathcount += 1;
    }

    for ( ; arg_index < argc; arg_index += 1) {
        nscount += 1;
    }
    */

    // Prepare hashtable and associated mutex
    hashtable* object_names_hashtable = init_table();
    pthread_mutex_t* hashtable_lock_ref = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(hashtable_lock_ref, NULL);

    // Prepare sync primitives to sync on active number of threads
    pthread_mutex_t* threads_active_lock_ref = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(threads_active_lock_ref, NULL);

    pthread_cond_t* threads_active_cv_ref = (pthread_cond_t*) calloc(1, sizeof(pthread_cond_t));
    pthread_cond_init(threads_active_cv_ref, NULL);

    // Prepare data and sync primitives for the pthread_t array to allow safe join, expansion, etc.
    pthread_t* thread_ids = (pthread_t*) calloc(max_threads, sizeof(pthread_t));
    pthread_mutex_t* thread_ids_lock_ref = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(thread_ids_lock_ref, NULL);    

    pthread_vector* pthread_ts_vector = pthread_vector_init(max_threads);


    // TODO: write hashtable contents to a file node-by-node before destroying
    destroy_table(object_names_hashtable);

}
