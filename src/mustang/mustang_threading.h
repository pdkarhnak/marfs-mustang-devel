#ifndef __MUSTANG_THREADING_H__
#define __MUSTANG_THREADING_H__

#include "hashtable.h"
#include "pthread_vector.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct threadcount_verifier_struct threadcount_verifier;

typedef struct threadcount_verifier_struct {
    pthread_mutex_t* self_lock;
    pthread_cond_t* active_threads_cv;
    size_t active_threads;
    size_t max_threads;
} threadcount_verifier;

typedef struct thread_args_struct thread_args;

typedef struct thread_args_struct {
    // Synchronization components to enforce a maximum number of active threads
    // at one time.
    threadcount_verifier* tc_verifier;

    pthread_vector* pt_vector;

    // MarFS context components for this thread: position and config
    /*
    marfs_position* this_position;
    marfs_config* this_config;
    */

    // Synchronization for the output hashtable of object names
    hashtable* hashtable;
    pthread_mutex_t* hashtable_lock;

    // After a verify_active_threads() call to put the thread to sleep as
    // needed until room is available, This will be the path that the new
    // thread chdir()-s into.
    char* basepath; 
    int parent_dirfd;

#ifdef DEBUG
    pthread_mutex_t* stdout_lock;
#endif

} thread_args;

threadcount_verifier* verifier_init(size_t threads_max);

void verifier_destroy(threadcount_verifier* verifier);

// thread_args* threadarg_init(threadcount_verifier* tc_verifier_new, pthread_vector* pt_vector_new, hashtable* hashtable_new, pthread_mutex_t* hashtable_lock, 

thread_args* threadarg_fork(thread_args* existing, char* new_basepath);

void threadarg_destroy(thread_args* args);

void verify_active_threads(threadcount_verifier* verifier);

void signal_active_threads(threadcount_verifier* verifier);

#endif
