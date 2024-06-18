#include "pthread_vector.h"

pthread_vector* pthread_vector_init(size_t new_capacity) {
    pthread_vector* new_vector = (pthread_vector*) calloc(1, sizeof(pthread_vector));

    if (new_vector == NULL || (errno == ENOMEM)) {
        // TODO: log error (failed to calloc() new vector/ENOMEM)
        return NULL;
    }

    new_vector->size = 0;
    new_vector->capacity = new_capacity;
    pthread_t* new_pthread_t_array = (pthread_t*) calloc(new_capacity, sizeof(pthread_t));

    if (new_pthread_t_array == NULL || (errno == ENOMEM)) {
        // TODO: log error (failed to calloc() vector's pthread_t array/ENOMEM)
        return NULL;
    }

    new_vector->pthread_id_array = new_pthread_t_array;
    
    pthread_mutex_t* new_array_lock = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
    int lock_init_code = pthread_mutex_init(new_array_lock, NULL);

    if (lock_init_code != 0) {
        // TODO: log error (macro?) (failed to init vector's mutex)
        return NULL;
    }

    new_vector->array_lock = new_array_lock;

    return new_vector;
}

pthread_t pthread_vector_get(pthread_vector* vector, size_t index) {
    if (vector == NULL) {
        // TODO: log warning (vector NULL -- ignoring call)
        errno = EINVAL;
        return 0;
    }

    pthread_t returned_id;

    pthread_mutex_lock(vector->array_lock);

    if ((index > vector->size) || (vector->pthread_id_array == NULL)) {
        // TODO: log error (bad index, or vector's pthread_id_array has already been destroyed)
        pthread_mutex_unlock(vector->array_lock);
        errno = EINVAL;
        return 0;
    }

    returned_id = (vector->pthread_id_array)[index];
    pthread_mutex_unlock(vector->array_lock);

    return returned_id;
}

/**
 * A private function to wrap size and capacity checks, as well as error 
 * handling on realloc() failure, for the public pthread_vector_appendset().
 *
 * DO NOT attempt to call this function directly--this function presumes a 
 * caller has already locked this vector's corresponding mutex, which a 
 * pthread_vector_appendset() call will satisfy but which a direct call will 
 * not.
 */
int pthread_vector_append(pthread_vector* vector, pthread_t new_thread_id) {
    if (vector->size >= vector->capacity) {
        vector->pthread_id_array = realloc(vector->pthread_id_array, (2 * (vector->capacity) * sizeof(pthread_t)));

        if (errno == ENOMEM) {
            // TODO: log error
            return -1;
        }

        vector->capacity *= 2;
    }

    (vector->pthread_id_array)[vector->size] = new_thread_id;
    vector->size += 1;

    return 0;
}

int pthread_vector_appendset(pthread_vector* vector, pthread_t* thread_ids, size_t count) {

    if (vector == NULL) {
        // TODO: log warning (vector NULL)
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(vector->array_lock);

    if (vector->pthread_id_array == NULL) {
        pthread_mutex_unlock(vector->array_lock);
        // TODO: log error (vector's pthread ID array has already been destroyed)
        errno = EINVAL;
        return -1;
    }
    
    int retval = 0;

    for (size_t index = 0; index < count; index += 1) {
        retval = pthread_vector_append(vector, thread_ids[index]);

        if (retval == -1) {
            pthread_mutex_unlock(vector->array_lock);
            // TODO: log error (could not append--ENOMEM)
            return retval;
        }
    }

    pthread_mutex_unlock(vector->array_lock);

    return retval;
}

int pthread_vector_pollthread(pthread_vector* vector, void** retval_ptr, size_t index) {
    pthread_t joined_thread = pthread_vector_get(vector, index);
    
    if ((joined_thread == 0) && (errno == EINVAL)) {
        return -1;
    }

    return pthread_join(joined_thread, retval_ptr);
}

void pthread_vector_destroy(pthread_vector* vector) {

    if (vector == NULL) {
        // TODO: log warning (vector NULL)
        errno = EINVAL;
        return;
    }

    pthread_mutex_lock(vector->array_lock);

    vector->size = 0;
    vector->capacity = 0;
    free(vector->pthread_id_array);
    vector->pthread_id_array = NULL;

    pthread_mutex_unlock(vector->array_lock);
    pthread_mutex_destroy(vector->array_lock);
    free(vector->array_lock);

    free(vector);
}
