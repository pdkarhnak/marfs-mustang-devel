#include "pthread_vector.h"
#include <stdlib.h>
#include <errno.h>

pthread_vector* pthread_vector_init(uint32_t new_capacity) {
    pthread_t* new_list = calloc(new_capacity, sizeof(pthread_t));

    if (new_list == NULL) {
        return NULL;
    }

    pthread_vector* new_vector = calloc(1, sizeof(pthread_vector));
    
    if (new_vector == NULL) {
        free(new_list);
        return NULL;
    }

    new_vector->threadlist = new_list;
    new_vector->size = 0;
    new_vector->capacity = new_capacity;

    return new_vector;
} 

int pthread_vector_append(pthread_vector* vector, pthread_t id) {
    if (vector == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (vector->size >= vector->capacity) {
        vector->threadlist = realloc(vector->threadlist, 2 * (vector->capacity) * sizeof(pthread_t));
        vector->capacity *= 2;

        if (vector->threadlist == NULL) {
            return -1;
        }
    }

    vector->threadlist[vector->size] = id;
    vector->size += 1;

    return 0;
}

int at_index(pthread_vector* vector, uint32_t index, pthread_t* to_ret) {
    if ((vector == NULL) || (to_ret == NULL) || (vector->threadlist == NULL)) {
        errno = EINVAL;

        if (to_ret != NULL) {
            *to_ret = 0;
        }

        return -1;
    }

    if (index > (vector->size)) {
        errno = EINVAL;
        *to_ret = 0;
        return -1;
    }

    *to_ret = (vector->threadlist)[index];
    return 0;
}

void pthread_vector_destroy(pthread_vector* vector) {
    free(vector->threadlist);
    vector->size = 0;
    free(vector);
}
