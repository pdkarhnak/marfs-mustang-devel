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

#include "retcode_ll.h"
#include <stdlib.h>
#include <errno.h>

retcode* node_init(char* new_basepath, RETCODE_FLAGS new_flags) {
    retcode* new_node = calloc(1, sizeof(retcode));
    
    if (new_node == NULL) {
        return NULL;
    }

    new_node->self = pthread_self();
    new_node->flags = new_flags;
    new_node->basepath = new_basepath;
    new_node->prev = NULL;
    new_node->next = NULL;

    return new_node;
}

retcode_ll* retcode_ll_init(void) {
    retcode_ll* new_ll = calloc(1, sizeof(retcode_ll));

    if (new_ll == NULL) {
        return NULL;
    }

    new_ll->size = 0;
    new_ll->head = NULL;
    new_ll->tail = NULL;

    return new_ll;
}

int retcode_ll_add(retcode_ll* rll, retcode* node) {
    if ((rll == NULL) || (node == NULL)) {
        errno = EINVAL;
        return -1;
    }

    if (rll->size == 0) {
        rll->head = node;
        rll->tail = node;
        rll->size += 1;
        return 0;
    }

    rll->tail->next = node;
    node->prev = rll->tail;
    rll->tail = node;
    rll->size += 1;
    return 0;
}

retcode_ll* retcode_ll_concat(retcode_ll* dest, retcode_ll* src) {
    if ((dest == NULL) || (src == NULL)) {
        return NULL;
    }

    if (dest->size == 0) {
        dest->head = src->head;
        dest->tail = src->tail;
        dest->size = src->size;
        free(src);
        return dest;
    }

    dest->tail->next = src->head;
    src->head->prev = dest->tail;
    dest->tail = src->tail;
    dest->size += src->size;

    free(src);

    return dest;
}

void retcode_ll_cleanlist(retcode* start) { 

    if (start == NULL) {
        return;
    }

    retcode* destroyed_node = start;

    do {
        retcode* next_ref = destroyed_node->next;
        destroyed_node->prev = NULL;
        destroyed_node->next = NULL;

        free(destroyed_node->basepath);
        free(destroyed_node);

        destroyed_node = next_ref;
    } while (destroyed_node != NULL);
}

void retcode_ll_flush(retcode_ll* rll, FILE* logfile, pthread_mutex_t* logfile_lock) {
    if (rll->size == 0) {
        return;
    }

    pthread_mutex_lock(logfile_lock);

    retcode* current_node = rll->head;

    do {
        retcode* next_ref = current_node->next;
        fprintf(logfile, "[thread %0lx]: exited with code %x\n", current_node->self, current_node->flags);
        current_node = next_ref;
    } while (current_node != NULL);

    pthread_mutex_unlock(logfile_lock);

    retcode_ll_cleanlist(rll->head);
    rll->size = 0;
    rll->head = NULL;
    rll->tail = NULL;
}

void retcode_ll_destroy(retcode_ll* rll) {
    if (rll == NULL) {
        // TODO: log warning (list NULL)
        errno = EINVAL;
        return;
    }

    retcode_ll_cleanlist(rll->head);

    rll->head = NULL;
    rll->tail = NULL;

    free(rll);
}
