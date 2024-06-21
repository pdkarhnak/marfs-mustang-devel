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

#ifndef __PTHREAD_VECTOR_H__
#define __PTHREAD_VECTOR_H__

#include <pthread.h>
#include <stdint.h>

#define DEFAULT_CAPACITY 8

/**
 * A very simple dynamic array to locally store pthread_ts corresponding to 
 * threads that a particular thread creates to traverse relevant 
 * subdirectories.
 *
 * NOTE: this vector is not thread-safe since it is not designed to be shared
 * between threads.
 */
typedef struct pthread_vector_struct {
    uint32_t size;
    uint32_t capacity;
    pthread_t* threadlist;
} pthread_vector;

/**
 * Initialize a vector on the heap according to `new_capacity`, including the
 * underlying pthread_t collection space.
 */
pthread_vector* pthread_vector_init(uint32_t new_capacity);

/**
 * Add a pthread_t to the given vector, "silently" expanding the vector's 
 * underlying collection space if needed.
 *
 * NOTE: for simplicity, this append operation (i.e., insert at tail) is the 
 * sole add operation implemented. This also keeps additions efficient with 
 * constant time complexity.
 *
 * Returns: 0 on success (add succeeded, including underlying resize 
 * operations), or -1 on failure.
 */
int pthread_vector_append(pthread_vector* vector, pthread_t id);

/**
 * Given a vector, retrieve the pthread_t stored at `index` within the vector's
 * underlying collection space.
 *
 * Returns: 0 on success with the pthread_t element stored in *to_ret, or -1 on
 * failure with errno set (EINVAL for bad vector, bad to_ret argument, or bad 
 * index).
 *
 * Users should check the integer return value first, then interpret the 
 * validity of *to_ret accordingly.
 */
int at_index(pthread_vector* vector, uint32_t index, pthread_t* to_ret);

/**
 * Destroy a vector, freeing any occupied space including the space for the 
 * underlying pthread_t collection.
 */
void pthread_vector_destroy(pthread_vector* vector);

#endif
