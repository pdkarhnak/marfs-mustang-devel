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

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <tagging/tagging.h>
#include "mustang_threading.h"

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>
#define ID_MASK 0xFFFFFFFF
#define SHORT_ID() (pthread_self() & ID_MASK)
#endif

thread_args* threadarg_init(marfs_config* shared_config, marfs_position* shared_position, hashtable* new_hashtable, 
        pthread_mutex_t* new_ht_lock, char* new_basepath, FILE* new_logfile, pthread_mutex_t* new_log_lock) {
    thread_args* new_args = (thread_args*) calloc(1, sizeof(thread_args));

    if ((new_args == NULL) || (errno == ENOMEM)) {
        return NULL;
    }

    new_args->base_config = shared_config;
    new_args->base_position = shared_position;
    new_args->hashtable = new_hashtable;
    new_args->hashtable_lock = new_ht_lock;
    new_args->basepath = new_basepath;
    new_args->log_ptr = new_logfile;
    new_args->log_lock = new_log_lock;

#ifdef DEBUG
    new_args->stdout_lock = NULL;
#endif

    return new_args;
}

RETCODE_FLAGS mustang_spawn(thread_args* existing, pthread_t* thread_id, marfs_position* new_position, char* new_basepath) {
    RETCODE_FLAGS flags = RETCODE_SUCCESS;

    thread_args* new_args = (thread_args*) calloc(1, sizeof(thread_args));

    if ((new_args == NULL) || (errno == ENOMEM)) {
        flags |= ALLOC_FAILED;
        return flags;
    }

    new_args->base_config = existing->base_config;
    new_args->base_position = new_position;
    new_args->hashtable = existing->hashtable;
    new_args->hashtable_lock = existing->hashtable_lock;

    new_args->basepath = new_basepath;
    new_args->log_ptr = existing->log_ptr;
    new_args->log_lock = existing->log_lock;

#ifdef DEBUG
    new_args->stdout_lock = existing->stdout_lock;
#endif

    int createcode = pthread_create(thread_id, NULL, &thread_main, (void*) new_args);

    if (createcode != 0) {
        flags |= PTHREAD_CREATE_FAILED;
    }

    return flags;
}

void threadarg_destroy(thread_args* args) {
    config_abandonposition(args->base_position);
    args->base_config = NULL;
    args->basepath = NULL;
    args->log_ptr = NULL;
    free(args);
}

char* get_ftag(marfs_position* current_position, MDAL current_mdal, char* path) {
    MDAL_FHANDLE target_handle = current_mdal->open(current_position->ctxt, path, O_RDONLY);
    if (handle == NULL) {
        return NULL;
    }

    char* ftag_str = NULL;
    // 1 == hidden xattr
    ssize_t ftag_len = current_mdal->fgetxattr(target_handle, 1, FTAG_NAME, ftag_str, 0); // Fetch initial length of ftag

    if (ftag_len <= 0) {
        current_mdal->close(target_handle);
        errno = ENOATTR;
        return NULL;
    }

    ftag_str = (char*) calloc((ftag_len + 1), sizeof(char));

    if (ftag_str == NULL) {
        current_mdal->close(target_handle);
        errno = ENOMEM;
        return NULL; 
    }

    if (current_mdal->fgetxattr(target_handle, 1, FTAG_NAME, ftagstr, ftag_len) != ftag_len) {
        current_mdal->close(target_handle);
        free(ftag_str);
        errno = ESTALE;
        return NULL;
    }

    current_mdal->close(target_handle);
    return ftag_str;
}
