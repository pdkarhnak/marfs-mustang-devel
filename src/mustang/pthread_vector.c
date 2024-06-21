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

#include "pthread_vector.h"
#include <stdlib.h>
#include <errno.h>

pthread_vector* pthread_vector_init(int new_capacity) {
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

int at_index(pthread_vector* vector, int index, pthread_t* to_ret) {
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
