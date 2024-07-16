#ifndef __MONITORS_H__
#define __MONITORS_H__

#include <pthread.h>
#include <stdlib.h>

typedef struct monitor_struct capacity_monitor_t;

typedef struct monitor_struct {
    size_t active;
    size_t capacity;
    pthread_mutex_t* lock;
    pthread_cond_t* cv;
} capacity_monitor_t;

capacity_monitor_t* monitor_init(size_t new_capacity, pthread_mutex_t* new_lock, pthread_cond_t* new_cv);

int monitor_procure(capacity_monitor_t* monitor);

int monitor_vend(capacity_monitor_t* monitor);

int monitor_destroy(capacity_monitor_t* monitor);

typedef struct countdown_monitor_struct countdown_monitor_t;

typedef struct countdown_monitor_struct {
    ssize_t active;
    pthread_mutex_t* lock;
} countdown_monitor_t;

countdown_monitor_t* countdown_monitor_init(pthread_mutex_t* new_lock);

int countdown_monitor_windup(countdown_monitor_t* ctdwn_monitor, ssize_t amount);

int countdown_monitor_decrement(countdown_monitor_t* ctdwn_monitor);

int countdown_monitor_wait(countdown_monitor_t* ctdwn_monitor);

int countdown_monitor_destroy(countdown_monitor_t* ctdwn_monitor);

#endif
