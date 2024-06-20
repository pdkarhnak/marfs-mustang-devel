#ifndef __PTHREAD_VECTOR_H__
#define __PTHREAD_VECTOR_H__

#include <pthread.h>
#include <stdint.h>

typedef struct pthread_vector_struct {
    uint32_t size;
    uint32_t capacity;
    pthread_t* threadlist;
} pthread_vector;

pthread_vector* pthread_vector_init(uint32_t new_capacity);

int pthread_vector_append(pthread_vector* vector, pthread_t id);

pthread_t at_index(pthread_vector* vector, uint32_t index);

void pthread_vector_destroy(pthread_vector* vector);

#endif
