/*
Copyright (c) 2015, Los Alamos National Security, LLC
All rights reserved.

Copyright 2015.  Los Alamos National Security, LLC. This software was
produced under U.S. Government contract DE-AC52-06NA25396 for Los
Alamos National Laboratory (LANL), which is operated by Los Alamos
National Security, LLC for the U.S. Department of Energy. The
U.S. Government has rights to use, reproduce, and distribute this
software.  NEITHER THE GOVERNMENT NOR LOS ALAMOS NATIONAL SECURITY,
LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LIABILITY
FOR THE USE OF THIS SOFTWARE.  If software is modified to produce
derivative works, such modified software should be clearly marked, so
as not to confuse it with the version available from LANL.
 
Additionally, redistribution and use in source and binary forms, with
or without modification, are permitted provided that the following
conditions are met: 1. Redistributions of source code must retain the
above copyright notice, this list of conditions and the following
disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. Neither the name of Los Alamos National Security, LLC, Los Alamos
National Laboratory, LANL, the U.S. Government, nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS
ALAMOS NATIONAL SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-----
NOTE:
-----
MarFS is released under the BSD license.

MarFS was reviewed and released by LANL under Los Alamos Computer Code
identifier: LA-CC-15-039.

MarFS uses libaws4c for Amazon S3 object communication. The original version
is at https://aws.amazon.com/code/Amazon-S3/2601 and under the LGPL license.
LANL added functionality to the original work. The original work plus
LANL contributions is found at https://github.com/jti-lanl/aws4c.

GNU licenses can be found at http://www.gnu.org/licenses/.
*/

#ifndef __MUSTANG_MONITORS_H__
#define __MUSTANG_MONITORS_H__

#include <pthread.h>
#include <stdlib.h>

typedef struct monitor_struct capacity_monitor_t;

/**
 * An bespoke implementation of a "monitor" synchronization primitive which can
 * enforce a limit on the number of currently active.
 */
typedef struct monitor_struct {
    size_t active;
    size_t capacity;
    pthread_mutex_t* lock;
    pthread_cond_t* cv;
} capacity_monitor_t;

/**
 * Initialize a "capacity monitor" (monitor to enforce a particular thread 
 * limit, stored in the capacity field) and its associated state, including its
 * mutex and condition variable.
 *
 * Returns: pointer to a valid and initialized capacity monitor struct on 
 * success, or NULL on failure. If new_capacity is specialized as zero, NULL is
 * returned with errno set as EINVAL.
 *
 * NOTE: the following conditions cause this function to return NULL:
 * - Bad capacity argument
 * - Failure to allocate heap memory for monitor
 * - Failure to allocate heap memory for mutex or to initialize mutex
 * - Failure to allocate heap memory for cv or to initialize cv
 *
 * It is important for the caller to check for a NULL return value because of 
 * these various conditions and report errno accordingly. 
 */
capacity_monitor_t* monitor_init(size_t new_capacity);

/**
 * Acquire the monitor's lock and atomically check whether the calling thread 
 * may increment the monitor's "active" field without that value exceeding the 
 * monitor's "capacity" field. If incrementing the "active" field would exceed 
 * capacity (meaning the monitor is currently at capacity), wait on the 
 * monitor's condition variable and put the thread to sleep.
 *
 * Returns: 0 at the conclusion of a successful wait, or -1 on error (monitor 
 * is NULL, and errno set to EINVAL).
 *
 * NOTE: this function will not return until the thread has been broadcast to 
 * on the condition variable **and** been able to successfully increment the 
 * monitor's "active" field without exceeding "capacity".
 *
 * NOTE: "procure" terminology based on "P" initial for semaphore operations.
 * See https://en.wikipedia.org/wiki/Semaphore_(programming)#Operation_names
 */
int monitor_procure(capacity_monitor_t* monitor);

/**
 * Acquire the monitor's lock and atomically decrement the monitor's "active" 
 * field to indicate that the calling thread is now "inactive" (but may still 
 * be alive and not yet exited). After decrementing the monitor's "active" 
 * field, broadcast on the cv so that waiting threads may wake up from their 
 * waits within monitor_procure() and attempt to become active (i.e., complete 
 * their wait and return).
 *
 * Returns: 0 on success, or -1 on failure (monitor is NULL, with errno set to 
 * EINVAL). Assuming a valid monitor that has been appropriately allocated and 
 * initialized (use monitor_init()), this function always succeeds.
 *
 * NOTE: "vend" terminology nased on "V" initial for semaphore operations. The
 * "V" initial is not new (see the above for "procure"), but it is the author's
 * knowledge that "vend" is an original coinage.
 */
int monitor_vend(capacity_monitor_t* monitor);

/**
 * Destroy the given monitor and its associated state, including its mutex and 
 * condition variable.
 *
 * Returns: 0 on success, or -1 on failure with errno set. EINVAL is set if the
 * monitor is NULL; EBUSY is set if the monitor's "active" field (the number of
 * threads currently interacting with the monitor or who may interact with the
 * monitor in the future) is nonzero.
 *
 * NOTE: calling this function in a manager thread repeatedly to "poll" worker 
 * threads is not a replacement for safely querying the number of threads who 
 * may use the monitor. Refer to the "countdown monitor" interface herein for 
 * one such case of that safe functionality.
 */
int monitor_destroy(capacity_monitor_t* monitor);

typedef struct countdown_monitor_struct countdown_monitor_t;

/**
 * A bespoke implementation of a "monitor" synchronization primitive/counting 
 * semaphore that communicates the number of live threads (i.e., threads who 
 * are using critical shared resources which therefore cannot be safely 
 * destroyed) at one moment in a mustang invocation. 
 *
 * Name chosen as an analogy to a dial-based kitchen timer. Threads can 
 * atomically "wind up" the timer (in the analogy, wind up the dial to add a 
 * minute or minutes to the timer) as well as "decrement" the timer (in the 
 * analogy, reflect the passage of time so that the number of minutes/seconds 
 * remaining deccreases).
 */
typedef struct countdown_monitor_struct {
    size_t active;
    pthread_mutex_t* lock;
} countdown_monitor_t;

/**
 * Allocate space for and initialize a countdown monitor, including its 
 * associated mutex. The monitor's "active" field is initialized to zero; if 
 * the caller wishes to indicate that they themselves are "alive", they should 
 * call countdown_monitor_windup() with a second argumento of 1 to do so.
 *
 * Returns: a pointer to a valid countdown monitor on success, or NULL on 
 * failure. 
 *
 * NOTE: this function may return NULL for the following conditions:
 * - Heap memory for the monitor's associated mutex could not be allocated.
 * - The monitor's associated mutex could not be initialized.
 * - Heap memory for the monitor could not be allocated.
 *
 * Callers are advised to check errno in addition to a NULL return code for a 
 * precise description of the error that occurred.
 */
countdown_monitor_t* countdown_monitor_init(void);

/**
 * Atomically add the specified amount to the countdown monitor's "active" 
 * field to indicate through the monitor that at least one more thread is 
 * "alive" and depends on critical shared state.
 *
 * Returns: 0 on success, or -1 on failure with errno set to EINVAL (monitor is
 * NULL).
 *
 * NOTE: it is possible to wind up a countdown monitor by amount 0, but this is
 * typically inadvisable for performance reasons (unnecessary locking) or 
 * safety reasons (the number of live threads is not correctly updated).
 */
int countdown_monitor_windup(countdown_monitor_t* ctdwn_monitor, size_t amount);

/**
 * Atomically decrement the countdown monitor's "active" field to indicate 
 * through the monitor that exactly one fewer thread is "alive" and depends on 
 * critical shared state. The post-decrement number of live threads is also 
 * atomically read out from the monitor and returned in *active_snapshot (if 
 * NULL is not passed as active_snapshot) for other thread purposes.
 *
 * Returns: 0 on success, or -1 on failure with errno set to EINVAL (monitor 
 * was NULL). The monitor's "active" field is atomically read out and returned 
 * in *active_snapshot if active_snapshot is not NULL.
 *
 * NOTE: While threads may decrement the counter to indicate they are "not 
 * alive", this merely indicates that they are not using the aforementioned 
 * critical shared resources; such threads may (and commonly will) still need 
 * to perform their own state cleanup and exit. A 0 value for the "active" 
 * field does NOT mean that it is safe to kill the process, but it does mean 
 * that the countdown monitor and any shared state whose use it was 
 * synchronizing may be safely destroyed.
 */
int countdown_monitor_decrement(countdown_monitor_t* ctdwn_monitor, size_t* active_snapshot);

/**
 * Destroy a countdown monitor and its associated mutex.
 *
 * Returns: 0 on success, or -1 on failure with errno set. EINVAL is set if the
 * monitor is NULL, and EBUSY is set if the monitor's "active" field does not 
 * equal zero.
 *
 * NOTE: this function should not be used to poll a countdown monitor for the 
 * number of active threads and repeatedly attempt to destroy it. Instead, use 
 * a combination of countdown_monitor_decrement() and other approaches to check
 * whether a countdown monitor may be destroyed.
 */
int countdown_monitor_destroy(countdown_monitor_t* ctdwn_monitor);

#endif
