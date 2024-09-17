/*******************************************************************************
 * Copyright (C) 2024 by Trapti Damodar Balgi
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. Users are
 * permitted to modify this and use it to learn about the field of embedded
 * software. Trapti Damodar Balgi and the University of Colorado are not liable for
 * any misuse of this material.
 * ****************************************************************************/

/**
 * @file    threading.c
 * @brief   Source file for threading implementation
 *
 * @author  Trapti Damodar Balgi
 * @date    09/16/2024
 * @references  
 * 1. AESD Lectures and Slides
 * 2. Linux System Programming - Chapter 7
 * 3. https://linux.die.net/man/3/pthread_mutex_init
 *
 */

#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    thread_data_t *thread_func_args = (thread_data_t*) thread_param;

    /*Sleep for wait_to_obtain_ms before locking mutex*/
    if(usleep(thread_func_args->wait_to_obtain_ms * 1000) != 0)
    {
        printf("Sleep for wait_to_obtain_ms before locking mutex failed!\n");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    /* Obtain the mutex, if return non-zero, lock failed*/
    if (pthread_mutex_lock (thread_func_args->mutex_t) != 0)
    {
        printf("Locking mutex failed!\n");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    /*Sleep for wait_to_release_ms before locking mutex*/
    if(usleep(thread_func_args->wait_to_release_ms * 1000) != 0)
    {
        printf("Sleep for wait_to_release_ms before locking mutex failed!\n");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    /*Unlock the mutex*/
    if (pthread_mutex_unlock (thread_func_args->mutex_t) != 0)
    {
        printf("Unlocking mutex failed!\n");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    /* Thread completed succesfully*/
    thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /* Args validity checks*/
    if ((thread == NULL) || (mutex == NULL) || (wait_to_obtain_ms <= 0) || (wait_to_release_ms <= 0))
        return false;
    
    /* Allocate memory for thread_data*/
    thread_data_t *thread_data_s = (thread_data_t*) malloc(sizeof(thread_data_t));
    if (thread_data_s == NULL)
    {
        printf("Malloc Failed!");
        return false;
    }

    /* Initialize the struct*/
    thread_data_s->mutex_t = mutex;
    thread_data_s->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data_s->wait_to_release_ms = wait_to_release_ms;
    thread_data_s->thread_complete_success = false;

    /*Create the thread and pass the structure to the thread*/
    if (pthread_create(thread, NULL, threadfunc, thread_data_s) != 0)
    {
        /* If ret non-zero, create failed*/
        printf("pthread_create Failed!");
        return false;
    }

    /*Thread created succesfully - join and mutex destroy taken care of in tests, return true*/
    return true;
}

