#include "mustang_monitors.h"
#include <limits.h>
#include <errno.h>
#include <sched.h>

capacity_monitor_t* monitor_init(size_t new_capacity) {
    if (new_capacity == 0) {
        errno = EINVAL;
        return NULL;
    }

    capacity_monitor_t* new_monitor = (capacity_monitor_t*) calloc(1, sizeof(capacity_monitor_t));

    if (new_monitor == NULL) {
        return NULL;
    }

    pthread_mutex_t* new_lock = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));

    if (new_lock == NULL) {
        free(new_monitor);
        return NULL;
    }

    errno = 0;
    errno = pthread_mutex_init(new_lock, NULL);

    if (errno) {
        free(new_monitor);
        free(new_lock);
        return NULL;
    }

    pthread_cond_t* new_cv = (pthread_cond_t*) calloc(1, sizeof(pthread_cond_t));

    if (new_cv == NULL) {
        free(new_monitor);
        pthread_mutex_destroy(new_lock);
        free(new_lock);
        return NULL;
    }

    errno = pthread_cond_init(new_cv, NULL);

    if (errno) {
        free(new_monitor);
        pthread_mutex_destroy(new_lock);
        free(new_lock);
        free(new_cv);
        return NULL;
    }

    new_monitor->active = 0;
    new_monitor->capacity = new_capacity;
    new_monitor->lock = new_lock;
    new_monitor->cv = new_cv;

    return new_monitor;
}

int monitor_procure(capacity_monitor_t* monitor) {
    if (monitor == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(monitor->lock);

    while (monitor->active >= monitor->capacity) {
        pthread_cond_wait(monitor->cv, monitor->lock);
    }

    monitor->active += 1;

    pthread_mutex_unlock(monitor->lock);

    return 0;
}

int monitor_vend(capacity_monitor_t* monitor) {
    if (monitor == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(monitor->lock);
    monitor->active -= 1;
    pthread_cond_broadcast(monitor->cv);
    pthread_mutex_unlock(monitor->lock);

    return 0;
}

int monitor_destroy(capacity_monitor_t* monitor) {
    if (monitor == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(monitor->lock);

    if (monitor->active > 0) {
        pthread_mutex_unlock(monitor->lock);
        errno = EBUSY;
        return -1;
    }

    pthread_mutex_unlock(monitor->lock);

    pthread_mutex_destroy(monitor->lock);
    free(monitor->lock);
    monitor->lock = NULL;

    pthread_cond_destroy(monitor->cv);
    free(monitor->cv);
    monitor->cv = NULL;

    free(monitor);

    return 0;
}

/** BEGIN countdown monitor implementation **/

countdown_monitor_t* countdown_monitor_init(void) {
    pthread_mutex_t* new_lock = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));

    if (new_lock == NULL) {
        return NULL;
    }

    errno = 0;
    errno = pthread_mutex_init(new_lock, NULL);

    if (errno) {
        free(new_lock);
        return NULL;
    }

    countdown_monitor_t* new_ctdwn_monitor = (countdown_monitor_t*) calloc(1, sizeof(countdown_monitor_t));
    
    if (new_ctdwn_monitor == NULL) {
        return NULL;
    }

    new_ctdwn_monitor->active = 0;
    new_ctdwn_monitor->lock = new_lock;

    return new_ctdwn_monitor;
}

int countdown_monitor_windup(countdown_monitor_t* ctdwn_monitor, ssize_t amount) {
    if ((ctdwn_monitor == NULL) || (amount < 0)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(ctdwn_monitor->lock);
    ctdwn_monitor->active += amount;
    pthread_mutex_unlock(ctdwn_monitor->lock);

    return 0;
}

int countdown_monitor_decrement(countdown_monitor_t* ctdwn_monitor) {
    if (ctdwn_monitor == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(ctdwn_monitor->lock);
    ctdwn_monitor->active -= 1;
    pthread_mutex_unlock(ctdwn_monitor->lock);

    return 0;
}

int countdown_monitor_wait(countdown_monitor_t* ctdwn_monitor) {
    if (ctdwn_monitor == NULL) {
        errno = EINVAL;
        return -1;
    }

    int wait_complete = 0;

    while (!wait_complete) {
        pthread_mutex_lock(ctdwn_monitor->lock);
        if (ctdwn_monitor->active == 0) {
            wait_complete = 1;
            pthread_mutex_unlock(ctdwn_monitor->lock);
        } else {
            pthread_mutex_unlock(ctdwn_monitor->lock);
            sched_yield();
        }
    }

    return 0;
}

int countdown_monitor_destroy(countdown_monitor_t* ctdwn_monitor) {
    if (ctdwn_monitor == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(ctdwn_monitor->lock);

    if (ctdwn_monitor->active != 0) {
        pthread_mutex_unlock(ctdwn_monitor->lock);
        errno = EBUSY;
        return -1;
    }

    pthread_mutex_unlock(ctdwn_monitor->lock);

    pthread_mutex_destroy(ctdwn_monitor->lock);
    free(ctdwn_monitor->lock);
    ctdwn_monitor->lock = NULL;
    free(ctdwn_monitor);

    return 0;
}
