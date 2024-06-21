#include "mustang_threading.h"

void* thread_main(void* args) {

    size_t retval = 0;
    thread_args* this_args = (thread_args*) args;

    verify_active_threads(this_args->tc_verifier);

    DIR* cwd_handle = fdopendir(this_args->cwd_fd);

#ifdef DEBUG
    if (cwd_handle == NULL) {
        pthread_mutex_lock(this_args->stdout_lock);
        printf("ERROR: directory handle is NULL! (%s)\n", strerror(errno));
        pthread_mutex_unlock(this_args->stdout_lock);
        pthread_exit((void*) 1);
    }
#endif

    struct dirent* current_entry = readdir(cwd_handle);

    // Maintain a local buffer of pthread_ts to limit locking on the pthread_vector and "flush" pthread_ts in bulk
    pthread_t new_thread_ids[16];
    int pts_count = 0; // an index to keep track of how many pthread_ts to flush at one time

    while (current_entry != NULL) {
        if (current_entry->d_type == DT_DIR) {

            // Skip current directory "." and parent directory ".." to avoid infinite loop in directory traversal
            if ( (strncmp(current_entry->d_name, ".", strlen(current_entry->d_name)) == 0) || (strncmp(current_entry->d_name, "..", strlen(current_entry->d_name)) == 0) ) {
                current_entry = readdir(cwd_handle);
                continue;
            }
  
            int next_cwd_fd = openat(this_args->cwd_fd, current_entry->d_name, O_RDONLY | O_DIRECTORY);

            thread_args* next_args = threadarg_fork(this_args, strndup(current_entry->d_name, strlen(current_entry->d_name)), next_cwd_fd);

            int createcode = pthread_create(&new_thread_ids[pts_count], NULL, &thread_main, (void*) next_args);

            if (createcode != 0) {
                // TODO: log warning/error: EAGAIN (no system resources available/system-wide limit on threads encountered)
                // Not strictly fail-deadly for this thread (just new threads will not be spawned)
                retval = EAGAIN;
            } else {
                pts_count += 1;
            }

#ifdef DEBUG
            if (createcode == 0) {
                pthread_mutex_lock(this_args->stdout_lock);
                printf("[thread %0lx]: forked new thread (ID: %0lx) at basepath %s\n", 
                        SHORT_ID(), (new_thread_ids[pts_count - 1] & 0xFFFF), current_entry->d_name);
                pthread_mutex_unlock(this_args->stdout_lock);
            }
#endif

            // If 16 threads have been spawned from this thread, "flush" the 
            // local buffer and add the corresponding pthread_ts to the shared 
            // vector
            if (pts_count == 16) {
                int addcode = pthread_vector_appendset(this_args->pt_vector, new_thread_ids, 16);
                pts_count = 0; // Unconditionally reset the pts_count to zero. The flush either completely succeeds or completely fails.

                if (addcode != 0) {
                    retval = addcode;
                    // TODO: log error
                }
            }

        } else if (current_entry->d_type == DT_REG) {
#ifdef DEBUG
            pthread_mutex_lock(this_args->stdout_lock);
            printf("[thread %0lx]: recording file [%s]/'%s' in hashtable.\n", SHORT_ID(), this_args->basepath, current_entry->d_name);
            pthread_mutex_unlock(this_args->stdout_lock);
#endif

            pthread_mutex_lock(this_args->hashtable_lock);
            put(this_args->hashtable, current_entry->d_name);
            pthread_mutex_unlock(this_args->hashtable_lock);

        } else {

            char* irregular_type;

            switch(current_entry->d_type) {
                case DT_BLK:
                    irregular_type = "block device";
                    break;
                case DT_CHR:
                    irregular_type = "character device";
                    break;
                case DT_FIFO:
                    irregular_type = "FIFO/pipe";
                    break;
                case DT_LNK:
                    irregular_type = "link";
                    break;
                case DT_SOCK:
                    irregular_type = "socket";
                    break;
                default:
                    irregular_type = "unknown";
            }

#ifdef DEBUG
            pthread_mutex_lock(this_args->stdout_lock);
            printf("[thread %0lx]: WARNING: ignoring irregular file \"%s\" (%s)\n", SHORT_ID(), current_entry->d_name, irregular_type);
            pthread_mutex_unlock(this_args->stdout_lock);
#endif
        }

        current_entry = readdir(cwd_handle);
    }

    // Flush all remaining pthread_ts of spawned threads to the shared vector
    int pt_flushcode = pthread_vector_appendset(this_args->pt_vector, new_thread_ids, pts_count);

    if (pt_flushcode != 0) {
        retval = pt_flushcode;
        // TODO: log error
    }

    signal_active_threads(this_args->tc_verifier);
    threadarg_destroy(this_args);
    closedir(cwd_handle);

    return ((void*) retval);

}
