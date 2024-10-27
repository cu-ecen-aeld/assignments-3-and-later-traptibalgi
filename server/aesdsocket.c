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
 * @file    aesdsocket.c
 * @brief   Source file for socket implementation
 *
 * @author  Trapti Damodar Balgi
 * @date    09/30/2024
 * @references  
 * 1. AESD Lectures and Slides
 * 2. https://beej.us/guide/bgnet/html/
 * 3. https://blog.taborkelly.net/programming/c/2016/01/09/sys-queue-example.html
 * 4. https://linux.die.net/man/2/clock_gettime
 * 5. https://man7.org/linux/man-pages/man2/clock_nanosleep.2.html
 * 6. 
 */

#define _POSIX_C_SOURCE 200112L  // Enable POSIX features

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <signal.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "queue.h"
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/select.h>
#include <unistd.h>

#define ERROR (-1)
#define BACKLOG (10)
#define PORT_NUM (9000)
#define BUF_INITIAL_SIZE (1024)
#define TIMESTAMP_INTERVAL (10)

/* Build switch */
#define USE_AESD_CHAR_DEVICE (1)

#if (USE_AESD_CHAR_DEVICE == 1)
    #define FILE_NAME "/dev/aesdchar"
#elif (USE_AESD_CHAR_DEVICE == 0)
	#define FILE_NAME "/var/tmp/aesdsocketdata"
#endif

int sockfd;
struct addrinfo *res;  // will point to the results
volatile sig_atomic_t caught_signal = 0;

/* The structure for the linked list that will manage server threads*/
typedef struct server_thread_params
{
    pthread_t thread_id;
    volatile bool thread_complete;
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];        /* Size for IPv4 addresses */
    pthread_mutex_t *tmp_file_write_mutex;
    SLIST_ENTRY(server_thread_params) link;
} server_thread_params_t;

/* The structure for the timestamp thread*/
typedef struct time_thread_params
{
    pthread_t thread_id;
    pthread_mutex_t *tmp_file_write_mutex;
} time_thread_params_t;

typedef SLIST_HEAD(socket_head,server_thread_params) head_t;

void cleanup() 
{
    if (sockfd != -1) 
    {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
    }

    #if (USE_AESD_CHAR_DEVICE == 0)
 	remove(FILE_NAME);
 	#endif

    if (res != NULL) 
    {
        freeaddrinfo(res);
    }

    closelog();
}

bool create_daemon ()
{
    bool daemon_status = false;
    pid_t pid = fork();
    int file;
    int ret_chdir, ret_stdin, ret_stdout, ret_stderr;

    if (pid < 0)
    {
        syslog(LOG_ERR, "Fork failed");
        return daemon_status;
    }

    if (pid > 0)
    {
        /*  Parent process, exit, allows daemon’s grandparent (init) to continue*/
        exit(0);
    }

    /* Create a new session */
    if (setsid() == -1)
    {
        syslog(LOG_ERR, "Failed to create a new session");
        return daemon_status;
    }

    /* Change the working directory and redirect std file descriptors*/ 
    if ((ret_chdir = chdir("/")) == ERROR)
    {
        syslog(LOG_ERR, "Failed to change directory");
    }

    if ((file = open("/dev/null", O_RDWR)) == ERROR)
    {
        syslog(LOG_ERR, "Failed to open /dev/null");
        return daemon_status;
    }

    if ((ret_stdin = dup2(file, STDIN_FILENO)) == ERROR) 
    {
        syslog(LOG_ERR, "Failed to redirect stdin");
    }


    if ((ret_stdout = dup2(file, STDOUT_FILENO)) == ERROR) 
    {
        syslog(LOG_ERR, "Failed to redirect stdout");
    }

    if ((ret_stderr = dup2(file, STDERR_FILENO)) == ERROR) 
    {
        syslog(LOG_ERR, "Failed to redirect stderr");
    }

    if ((ret_chdir != ERROR) && (ret_stdin != ERROR) && (ret_stdout != ERROR) && (ret_stderr != ERROR))
    {
        daemon_status = true;
    }
    close(file);
    return daemon_status;
}

static void signal_handler (int signal_number)
{
    caught_signal = signal_number;
}

#if (USE_AESD_CHAR_DEVICE == 0)
void *threadfn_timestamp(void *time_thread_params_struct)
{
    time_thread_params_t *time_params = (time_thread_params_t*)time_thread_params_struct;
    if (time_params == NULL)
    {
        syslog(LOG_ERR, "Time thread struct is NULL");
        return NULL;
    }

    struct timespec wall_time;
    char outstr[300];
    time_t t;
    struct tm *tmp;

    /* Run until a signal caught */
    while(!caught_signal)
    {
        t = time(NULL);
        tmp = localtime(&t);
        if (tmp == NULL) 
        {
            syslog(LOG_ERR, "Localtime");
            return NULL;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &wall_time) != 0)
        {
            syslog(LOG_ERR, "Failed clock_gettime");
            continue;
        }

        wall_time.tv_sec += TIMESTAMP_INTERVAL;

        /* Sleep for 10 seconds */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wall_time, NULL);

        if (strftime(outstr, sizeof(outstr),"timestamp: %Y/%m/%d %H:%M:%S\n", tmp) == 0) 
        {
            syslog(LOG_ERR, "strftime failed");
            continue;
        }

        FILE *file_write = fopen(FILE_NAME, "a+");
        if (file_write == NULL)
        {
            syslog(LOG_ERR, "Failed to open write file");
            return NULL;
        }

        /* Lock mutex before writing to tmp file and set file position*/
        int write_bytes = 0;
        pthread_mutex_lock(time_params->tmp_file_write_mutex);
        fseek(file_write, 0, SEEK_END);
        write_bytes = fwrite(outstr, sizeof(char), strlen(outstr), file_write);
        fflush(file_write);
        pthread_mutex_unlock(time_params->tmp_file_write_mutex);
        if (write_bytes < 0)
        {
            syslog(LOG_ERR, "Timestamp write failed");
            continue;
        }

        if (file_write != NULL) 
        {
            fclose(file_write);
            file_write = NULL;
        }
    }
    return NULL;
}
#endif

int receive_data(server_thread_params_t *server_params, char **buf, size_t *receive_buf_size)
{
    syslog(LOG_DEBUG, "in receive_data");
    int length;
    size_t total_received = 0;
    char *end_packet = NULL;

    do 
    {
        if (total_received + 1 >= *receive_buf_size)
        {
            *receive_buf_size *= 2;
            char *new_buf = realloc(*buf, *receive_buf_size);
            if (new_buf == NULL)
            {
                syslog(LOG_ERR, "Realloc failed for receiving buffer");
                return -1;
            }
            memset(new_buf + total_received, 0, *receive_buf_size - total_received);
            *buf = new_buf;
        }

        length = recv(server_params->client_fd, *buf + total_received, *receive_buf_size - total_received - 1, 0);
        if (length == -1)
        {
            syslog(LOG_ERR, "Receive failed");
            return -1;
        }

        total_received += length;
        end_packet = strchr(*buf, '\n');

    } while (end_packet == NULL && length > 0);

    return (end_packet != NULL) ? 0 : -1;
}

int process_data(server_thread_params_t *server_params, char *buf, size_t receive_buf_size)
{
    syslog(LOG_DEBUG, "in process_data");
    char *end_packet = strchr(buf, '\n');
    if (end_packet == NULL)
    {
        syslog(LOG_ERR, "Received without newline");
        return -1;
    }

    size_t valid_size = end_packet - buf + 1;
    buf[valid_size] = '\0';

    FILE *file_write = fopen(FILE_NAME, "a+");
    if (file_write == NULL)
    {
        syslog(LOG_ERR, "Failed to open write file");
        return -1;
    }

    pthread_mutex_lock(server_params->tmp_file_write_mutex);
    fseek(file_write, 0, SEEK_END);
    size_t written_bytes = fwrite(buf, sizeof(char), valid_size, file_write);
    fflush(file_write);
    pthread_mutex_unlock(server_params->tmp_file_write_mutex);

    if (written_bytes < valid_size)
    {
        syslog(LOG_ERR, "Write to temp file failed");
        if (file_write != NULL) 
        {
            fclose(file_write);
            file_write = NULL;
        }
        return -1;
    }

    if (file_write != NULL) 
    {
        fclose(file_write);
        file_write = NULL;
    }
    return 0;
}

int send_response(server_thread_params_t *server_params, char *buf, size_t receive_buf_size)
{
    syslog(LOG_DEBUG, "in send_response");
    size_t read_bytes;

    FILE *file_write = fopen(FILE_NAME, "a+");
    if (file_write == NULL)
    {
        syslog(LOG_ERR, "Failed to open write file");
        return -1;
    }

    pthread_mutex_lock(server_params->tmp_file_write_mutex);
    fseek(file_write, 0, SEEK_SET);
    while ((read_bytes = fread(buf, sizeof(char), receive_buf_size - 1, file_write)) > 0)
    {
        syslog(LOG_INFO, "Read %s from file", buf);
        fflush(file_write);
        if (send(server_params->client_fd, buf, read_bytes, 0) == -1) 
        {
            syslog(LOG_ERR, "Send to client failed");
            pthread_mutex_unlock(server_params->tmp_file_write_mutex);
            if (file_write != NULL) 
            {
                fclose(file_write);
                file_write = NULL;
            }
            return -1;
        }
    }
    pthread_mutex_unlock(server_params->tmp_file_write_mutex);
    if (file_write != NULL) 
    {
        fclose(file_write);
        file_write = NULL;
    }
    return 0;
}

void *threadfn_server(void *server_thread_params_struct)
{
    syslog(LOG_DEBUG, "in thread");
    server_thread_params_t *server_params = (server_thread_params_t*)server_thread_params_struct;
    char *buf = NULL;
    int result = 0;

    if (server_params == NULL)
    {
        syslog(LOG_ERR, "Thread server_thread_params is NULL");
        return NULL;
    }

    do 
    {
        size_t receive_buf_size = BUF_INITIAL_SIZE;
        buf = malloc(receive_buf_size);
        if (buf == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for receiving buffer");
            result = -1;
            break;
        }
        memset(buf, 0, receive_buf_size);

        result = receive_data(server_params, &buf, &receive_buf_size);
        if (result != 0) break;

        result = process_data(server_params, buf, receive_buf_size);
        if (result != 0) break;

        result = send_response(server_params, buf, receive_buf_size);

    } while (0);

    free(buf);
    close(server_params->client_fd);
    syslog(LOG_DEBUG, "Closed connection from %s", server_params->client_ip);
    server_params->thread_complete = true;
    return (void*)(intptr_t)result;
}

int main ( int argc, char **argv )
{
    openlog("socket", LOG_PID | LOG_CONS, LOG_USER);
    bool is_daemon = false;

    if ((argc >= 2) && strcmp(argv[1], "-d") == 0) 
    {
        is_daemon = true;
    } 

    /* Lines 363 - 382 were referenced from https://beej.us/guide/bgnet/html/ */
    int status;
    socklen_t addr_size;
    struct addrinfo hints;
    struct sockaddr_storage their_addr;

    memset(&hints, 0, sizeof hints);    // Make sure the struct is empty
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;        // Fill in my IP for me

    if ((status = getaddrinfo(NULL, "9000", &hints, &res)) != 0) 
    {
        syslog(LOG_ERR, "getaddrinfo failed");
        cleanup();
        exit(1);
    }

    /* Create a socket */
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        syslog(LOG_ERR, "Failed to make a socket");
        cleanup();
        exit(1);
    }
    
    /* Allow reuse of socket */
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != 0)
    {
        syslog(LOG_ERR, "Socket reuse failed");
        cleanup();
        exit(1);
    }

    /* Bind it to the port we passed in to getaddrinfo(): */
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) 
    {
        syslog(LOG_ERR, "Bind failed");
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        cleanup();
        exit(1);
    }

    /* Run as a daemon if specified */
    if (is_daemon)
    {
        bool daemon_status = create_daemon();
        if (!daemon_status)
        {
            cleanup();
            exit(1);
        }
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        syslog(LOG_ERR, "Listen failed");
        cleanup();
        exit(1);
    }

    /* Setup signal handlers*/
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &new_action, NULL) != 0)
    {
        syslog(LOG_ERR, "Sigaction for SIGTERM failed");
    }

    if (sigaction(SIGINT, &new_action, NULL))
    {
        syslog(LOG_ERR, "Sigaction for SIGINT failed");
    }

    pthread_mutex_t tmp_file_write_mutex;
    /* Create a mutex for synchronising writes to tmp_file*/
    if(pthread_mutex_init(&tmp_file_write_mutex, NULL) != 0)
    {
        syslog(LOG_ERR, "Creating file write mutex failed");
        cleanup();
        exit(1);
    }

    #if (USE_AESD_CHAR_DEVICE == 0)
    time_thread_params_t *time_params = NULL;
    time_params = (time_thread_params_t*)malloc(sizeof(time_thread_params_t));
    if(time_params == NULL)
    {
        syslog(LOG_ERR, "Malloc for time thread structure failed");
        cleanup();
        exit(1);
    }

    time_params->tmp_file_write_mutex = &tmp_file_write_mutex;
    
    if ((pthread_create(&(time_params->thread_id), NULL, &threadfn_timestamp, (void*)time_params)) != 0)
    {
        syslog(LOG_ERR, "Timestamp thread creation failed");
        cleanup();
        exit(1);
    }
    #endif

    /* Initialize the head */
    head_t head;
    SLIST_INIT(&head); 

    /* Create the server thread*/
    server_thread_params_t *server_params = NULL;

    /* Now accept incoming connections in a loop while signal not caught*/
    while (!caught_signal)
    {
        int new_fd;
        char client_ip[INET_ADDRSTRLEN];     
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in*)&their_addr)->sin_addr), client_ip, sizeof(client_ip));
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        server_params = (server_thread_params_t*)malloc(sizeof(server_thread_params_t));
        if(server_params == NULL)
        {
            syslog(LOG_ERR, "Malloc for server thread params failed");
            continue;
        }

        server_params->thread_complete = false;
        server_params->client_fd = new_fd;
        strncpy(server_params->client_ip, client_ip, INET_ADDRSTRLEN);
        server_params->tmp_file_write_mutex = &tmp_file_write_mutex;
        
        if ((pthread_create(&(server_params->thread_id), NULL, threadfn_server, (void*)server_params)) != 0)
        {
            syslog(LOG_ERR, "Thread creation failed");
            free(server_params);
            server_params = NULL;
            continue;
        }

        /* Add the node to the SLIST*/
        SLIST_INSERT_HEAD(&head, server_params, link);

        /* Attempt to join threads by checking for the complete_thread flag*/
        server_thread_params_t *iterator = NULL;
        server_thread_params_t *tmp = NULL;

        SLIST_FOREACH_SAFE(iterator, &head, link, tmp) 
	    {
            if (iterator->thread_complete == true) 
	        {
                if(pthread_join(iterator->thread_id, NULL) != 0)
		        {
                    syslog(LOG_ERR, "Thread join failed for %ld", iterator->thread_id);
                }
                syslog(LOG_INFO, "Thread joined %ld", iterator->thread_id);

                /* Remove node from the list and free the memory */
                SLIST_REMOVE(&head, iterator, server_thread_params, link);
                free(iterator);
                iterator = NULL;
            }
        }
    }

    /* Cleanup after caught signal */
    /* Mutex */
    pthread_mutex_destroy(&tmp_file_write_mutex);
    #if (USE_AESD_CHAR_DEVICE == 0)
    /* Timestamp thread */
    pthread_cancel(time_params->thread_id);
    pthread_join(time_params->thread_id, NULL);
    free(time_params);
    #endif
    /* Server thread */
    server_thread_params_t *iterator = NULL;
    server_thread_params_t *tmp = NULL;
    SLIST_FOREACH_SAFE(iterator, &head, link, tmp) 
    {
        if(pthread_join(iterator->thread_id, NULL) != 0)
        {
            syslog(LOG_ERR, "Thread join failed for %ld", iterator->thread_id);
        }
        syslog(LOG_INFO, "Thread joined %ld", iterator->thread_id);

        /* Remove node from the list and free the memory */
        SLIST_REMOVE(&head, iterator, server_thread_params, link);
        free(iterator);
        iterator = NULL;
    }
    cleanup();
    closelog();
}