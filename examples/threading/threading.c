#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg, ...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void *threadfunc(void *thread_param)
{
    struct thread_data *thread_func_args = (struct thread_data *)thread_param;
    bool return_flag = true;

    if (usleep((thread_func_args->wait_to_obtain_ms) * 1000) == -1)
    {
        ERROR_LOG("Error in mutex lock with code: %d", errno);
        return_flag = false;
    }

    if (pthread_mutex_lock(thread_func_args->mutex) != 0)
    {
        ERROR_LOG("Error in mutex lock with code: %d", errno);
        return_flag = false;
    }

    if (usleep((thread_func_args->wait_to_release_ms) * 1000) == -1)
    {
        ERROR_LOG("Error in mutex lock with code: %d", errno);
        return_flag = false;
    }

    if (pthread_mutex_unlock(thread_func_args->mutex) != 0)
    {
        ERROR_LOG("Error in mutex lock with code: %d", errno);
        return_flag = false;
    }

    if (return_flag)
        thread_func_args->thread_complete_success = true;
    else
        thread_func_args->thread_complete_success = false;

    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *args_to_thread;

    args_to_thread = malloc(sizeof(struct thread_data));

    args_to_thread->mutex = mutex;
    args_to_thread->thread_complete_success = false;
    args_to_thread->wait_to_obtain_ms = wait_to_obtain_ms;
    args_to_thread->wait_to_release_ms = wait_to_release_ms;

    int rc = pthread_create(thread, NULL, threadfunc, args_to_thread);

    if (!rc)
    {
        printf("Successfully created the thread \n");
        return true;
    }
    else
    {
        ERROR_LOG("failed to successfully create the thread \n");
    }

    return false;
}
