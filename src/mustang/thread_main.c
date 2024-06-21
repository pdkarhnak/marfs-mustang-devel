#include "mustang_threading.h"
#include "pthread_vector.h"
#include "retcode_ll.h"

void* thread_main(void* args) {

    thread_args* this_args = (thread_args*) args;

    retcode* this_retcode = node_init(this_args->basepath, SUCCESS);
    retcode_ll* this_ll = retcode_ll_init();

    if ((this_retcode) == NULL || (this_ll == NULL)) {
        threadarg_destroy(this_args);
        return NULL; // TODO: correspondingly handle this from the parent side
    }

    DIR* cwd_handle = fdopendir(this_args->cwd_fd);

    if (cwd_handle == NULL) {
        this_retcode->flags |= DIR_OPEN_FAILED;
        retcode_ll_add(this_retcode);

#ifdef DEBUG
        pthread_mutex_lock(this_args->stdout_lock);
        printf("ERROR: directory handle is NULL! (%s)\n", strerror(errno));
        pthread_mutex_unlock(this_args->stdout_lock);
#endif

        threadarg_destroy(this_args);
        return (void*) this_ll;
    }

    pthread_vector* spawned_threads = pthread_vector_init(DEFAULT_CAPACITY);

    if (spawned_threads == NULL) {
        this_retcode->flags |= ALLOC_FAILED;
        retcode_ll_add(this_retcode);
        closedir(cwd_handle);
        threadarg_destroy(this_args);
        return (void*) this_ll;
    }

    struct dirent* current_entry = readdir(cwd_handle);

    while (current_entry != NULL) {
        if (current_entry->d_type == DT_DIR) {

            // Skip current directory "." and parent directory ".." to avoid infinite loop in directory traversal
            if ( (strncmp(current_entry->d_name, ".", strlen(current_entry->d_name)) == 0) || (strncmp(current_entry->d_name, "..", strlen(current_entry->d_name)) == 0) ) {
                current_entry = readdir(cwd_handle);
                continue;
            }
  
            int next_cwd_fd = openat(this_args->cwd_fd, current_entry->d_name, O_RDONLY | O_DIRECTORY);

            if (next_cwd_fd == -1) {
                this_retcode->flags |= NEW_DIRFD_OPEN_FAILED;
                current_entry = readdir(cwd_handle);
                continue;
            }

            thread_args* next_args = threadarg_fork(this_args, strndup(current_entry->d_name, strlen(current_entry->d_name)), next_cwd_fd);

            if (next_args == NULL) {
                this_retcode->flags |= THREADARG_FORK_FAILED;
                close(next_cwd_fd);
                current_entry = readdir(cwd_handle);
                continue;
            }

            pthread_t next_id;
            int createcode = pthread_create(&next_id, NULL, &thread_main, (void*) next_args);

            if (createcode != 0) {
                // TODO: log warning/error: EAGAIN (no system resources available/system-wide limit on threads encountered)
                // Not strictly fail-deadly for this thread (just new threads will not be spawned)
                this_retcode |= PTHREAD_CREATE_FAILED;
                close(next_cwd_fd);
            } else {
                pthread_vector_append(spawned_threads, next_id);
            } 
#ifdef DEBUG
            if (createcode == 0) {
                pthread_mutex_lock(this_args->stdout_lock);
                printf("[thread %0lx]: forked new thread (ID: %0lx) at basepath %s\n", 
                        SHORT_ID(), (new_thread_ids[pts_count - 1] & 0xFFFF), current_entry->d_name);
                pthread_mutex_unlock(this_args->stdout_lock);
            }
#endif

        } else if (current_entry->d_type == DT_REG) {
#ifdef DEBUG
            pthread_mutex_lock(this_args->stdout_lock);
            printf("[thread %0lx]: recording file [%s]/'%s' in hashtable.\n", SHORT_ID(), this_args->basepath, current_entry->d_name);
            pthread_mutex_unlock(this_args->stdout_lock);
#endif

            pthread_mutex_lock(this_args->hashtable_lock);
            put(this_args->hashtable, current_entry->d_name);
            pthread_mutex_unlock(this_args->hashtable_lock);

        }

        current_entry = readdir(cwd_handle);
    }

    pthread_t to_join;

    for (uint32_t thread_index = 0; thread_index < spawned_threads->size; thread_index += 1) {

        int findcode = at_index(spawned_threads, thread_index, &to_join);

        if (findcode == -1) {
            continue;
        }

        retcode_ll* joined_ll; 
        int joincode = pthread_join(to_join, &joined_ll);

        if (joincode != 0) {
            this_retcode->flags |= PTHREAD_JOIN_FAILED;
            continue;
        }

        if (joined_ll == NULL) {
            this_retcode->flags |= CHILD_ALLOC_FAILED;
            continue;
        }

        this_ll = retcode_ll_concat(this_ll, joined_ll);

        if (this_ll->size >= RC_LL_LEN_MAX) {
            retcode_ll_flush(this_ll, this_args->log_ptr, log_lock);
        }

    }

    threadarg_destroy(this_args);
    closedir(cwd_handle);

    return (void*) this_ll;

}
