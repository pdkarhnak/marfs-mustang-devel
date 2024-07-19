#include "mustang_task_queue.h"
#include <errno.h>
#include <string.h>

mustang_task* task_init(marfs_config* task_config, marfs_position* task_position, hashtable* task_ht, pthread_mutex_t* task_ht_lock, void (*traversal_routine)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*)) {
    mustang_task* new_task = (mustang_task*) calloc(1, sizeof(mustang_task));

    if (new_task == NULL) {
        return NULL;
    }

    new_task->config = task_config;
    new_task->position = task_position;
    new_task->ht = task_ht;
    new_task->ht_lock = task_ht_lock;
    new_task->traversal_func = traversal_routine;
    new_task->prev = NULL;
    new_task->next = NULL;

    return new_task;
}

task_queue* task_queue_init(size_t new_capacity) {
    if (new_capacity == 0) {
        errno = EINVAL;
        return NULL;
    }

    task_queue* new_queue = (task_queue*) calloc(1, sizeof(task_queue));

    if (new_queue == NULL) {
        return NULL;
    }
    
    new_queue->capacity = new_capacity;
    new_queue->size = 0;
    new_queue->todos = 0;
    new_queue->head = NULL;
    new_queue->tail = NULL;

    pthread_mutex_t* new_queue_lock = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));

    if (new_queue_lock == NULL) {
        free(new_queue);
        return NULL;
    }

    if (pthread_mutex_init(new_queue_lock, NULL)) {
        free(new_queue_lock);
        free(new_queue);
        return NULL;
    }

    new_queue->lock = new_queue_lock;

    pthread_cond_t* new_tasks_cv = (pthread_cond_t*) calloc(1, sizeof(pthread_cond_t));

    if (new_tasks_cv == NULL) {
        pthread_mutex_destroy(new_queue_lock);
        free(new_queue_lock);
        free(new_queue);
        return NULL;
    }

    if (pthread_cond_init(new_tasks_cv, NULL)) {
        free(new_tasks_cv);
        pthread_mutex_destroy(new_queue_lock);
        free(new_queue_lock);
        free(new_queue);
        return NULL;
    }

    new_queue->task_available = new_tasks_cv;

    pthread_cond_t* new_space_cv = (pthread_cond_t*) calloc(1, sizeof(pthread_cond_t));

    if (new_space_cv == NULL) { 
        pthread_cond_destroy(new_tasks_cv);
        free(new_tasks_cv);
        pthread_mutex_destroy(new_queue_lock);
        free(new_queue_lock);
        free(new_queue);
        return NULL;
    }

    if (pthread_cond_init(new_space_cv, NULL)) {
        free(new_space_cv);
        pthread_cond_destroy(new_tasks_cv);
        free(new_tasks_cv);
        pthread_mutex_destroy(new_queue_lock);
        free(new_queue_lock);
        free(new_queue);
        return NULL;
    }

    new_queue->space_available = new_space_cv;

    pthread_cond_t* new_manager_cv = (pthread_cond_t*) calloc(1, sizeof(pthread_cond_t));

    if (new_manager_cv == NULL) {
        pthread_cond_destroy(new_space_cv);
        free(new_space_cv);
        pthread_cond_destroy(new_tasks_cv);
        free(new_tasks_cv);
        pthread_mutex_destroy(new_queue_lock);
        free(new_queue_lock);
        free(new_queue);
        return NULL;
    }

    if (pthread_cond_init(new_manager_cv, NULL)) {
        free(new_manager_cv);
        pthread_cond_destroy(new_space_cv);
        free(new_space_cv);
        pthread_cond_destroy(new_tasks_cv);
        free(new_tasks_cv);
        pthread_mutex_destroy(new_queue_lock);
        free(new_queue_lock);
        free(new_queue);
        return NULL;
    }

    new_queue->manager_cv = new_manager_cv;

    return new_queue;
}

int task_enqueue(task_queue* queue, mustang_task* new_task) {
    if ((queue == NULL) || (new_task == NULL)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(queue->lock);

    while (queue->size >= queue->capacity) {
        pthread_cond_wait(queue->space_available, queue->lock);
    }

    if (queue->tail != NULL) {
        queue->tail->next = new_task;
        new_task->prev = queue->tail;
    }

    queue->tail = new_task;
    queue->size += 1;
    queue->todos += 1;

    if (queue->size == 1) {
        queue->head = new_task;
    }

    pthread_cond_signal(queue->task_available);
    pthread_mutex_unlock(queue->lock);

    return -1;
}

mustang_task* task_dequeue(task_queue* queue) {
    if (queue == NULL) {
        errno = EINVAL;
        return NULL;
    }

    mustang_task* retrieved_task;

    pthread_mutex_lock(queue->lock);

    while (queue->size == 0) {
        pthread_cond_wait(queue->task_available, queue->lock);
    }

    retrieved_task = queue->head;
    queue->head = queue->head->next;
    queue->size -= 1;

    if (queue->head != NULL) {
        queue->head->prev = NULL;
    }

    if (queue->size == 0) {
        queue->tail = NULL;
    }

    retrieved_task->next = NULL;

    pthread_cond_signal(queue->space_available);
    pthread_mutex_unlock(queue->lock);

    return retrieved_task;
}

int task_queue_destroy(task_queue* queue) {
    if (queue->size > 0) {
        errno = EBUSY;
        return -1;
    }

    pthread_mutex_destroy(queue->lock);
    free(queue->lock);

    pthread_cond_destroy(queue->task_available);
    free(queue->task_available);
    
    pthread_cond_destroy(queue->space_available);
    free(queue->space_available);

    pthread_cond_destroy(queue->manager_cv);
    free(queue->manager_cv);

    free(queue);
    return 0;
}
