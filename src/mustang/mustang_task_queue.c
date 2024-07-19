#include <string.h>
#include <errno.h>

mustang_task* task_init(marfs_config* task_config, marfs_position* task_position, hashtable* task_ht, pthread_mutex_t* task_ht_lock,
        void (*traversal_func)(marfs_config*, marfs_position*, hashtable*, pthread_mutex_t*)) {
    mustang_task* new_task = (mustang_task*) calloc(1, sizeof(mustang_task));
    
    if (new_task == NULL) {
        return NULL;
    }

    new_task->config = task_config;
    new_task->position = task_position;
    new_task->ht = task_ht;
    new_task->ht_lock = task_ht_lock;
    new_task->traversal_task = traversal_func;
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

    new_queue->size = 0;
    new_queue->capacity = new_capacity;
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

    new_queue->lock = new_queue_lock;
    new_queue->task_available = new_tasks_cv;
    new_queue->space_available = new_space_cv;

    return new_queue;
}

int task_enqueue(task_queue* queue, mustang_task* new_task) {
    if ((queue == NULL) || (new_task == NULL)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(queue->lock);

    if (queue->size == 0) {
        queue->head = new_task;
        queue->tail = new_task;
        queue->size += 1;
        queue->todos += 1;
        pthread_cond_signal(queue->task_available);
        pthread_mutex_unlock(queue->lock);
        return 0;
    }

    while (queue->size >= queue->capacity) {
        pthread_cond_wait(queue->space_available, queue->lock);
    }

    queue->tail->next = new_task;
    new_task->prev = queue->tail;
    queue->tail = new_task;
    queue->size += 1;
    queue->todos += 1;
    pthread_cond_signal(queue->task_available);
    pthread_mutex_unlock(queue->lock);

    return 0;
}

mustang_task* task_dequeue(task_queue* queue) {
    if (queue == NULL) {
        errno = EINVAL; 
        return NULL;
    }

    mustang_task* retrieved_task = NULL;

    pthread_mutex_lock(queue->lock);

    while (queue->size == 0) {
        pthread_cond_wait(queue->task_available, queue->lock);
    }

    retrieved_task = queue->head;
    queue->head = retrieved_task->next;

    if (queue->head != NULL) {
        queue->head->prev = NULL;
    }

    queue->size -= 1;
    pthread_cond_signal(queue->space_available);
    pthread_mutex_unlock(queue->lock);

    retrieved_task->next = NULL;
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

    free(queue);
    return 0;
}
