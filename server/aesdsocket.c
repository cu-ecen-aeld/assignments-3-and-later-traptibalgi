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
 *
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

#define ERROR (-1)
#define BACKLOG (10)
#define PORT_NUM (9000)
#define BUF_SIZE (1024)

int sockfd, new_fd;
struct addrinfo *res;  // will point to the results
volatile sig_atomic_t caught_signal = 0;
FILE *tmp_file = NULL;

void cleanup() 
{
    if (new_fd != -1) 
    {
        shutdown(new_fd, SHUT_RDWR);
        close(new_fd);
    }

    if (sockfd != -1) 
    {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
    }

    if (tmp_file != NULL) 
    {
        fclose(tmp_file);
        tmp_file = NULL;
        remove("/var/tmp/aesdsocketdata");
    }

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

int main ( int argc, char **argv )
{
    openlog("socket", LOG_PID | LOG_CONS, LOG_USER);
    bool is_daemon = false;

    if ((argc >= 2) && strcmp(argv[1], "-d") == 0) 
    {
        is_daemon = true;
    } 

    /* Lines 104 - 124 were referenced from https://beej.us/guide/bgnet/html/ */
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

    tmp_file = fopen("/var/tmp/aesdsocketdata", "w+");
    if (tmp_file == NULL) 
    {
        syslog(LOG_ERR, "Failed to open /var/tmp/aesdsocketdata");
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

    /* Now accept an incoming connection in a loop while signal not caught*/
    while (!caught_signal)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            cleanup();
            exit(1);
        }
    
        char client_ip[INET_ADDRSTRLEN];        /* Size for IPv4 addresses */
        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in*)&their_addr)->sin_addr), client_ip, sizeof(client_ip));
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        /* Dynamically allocate an initial buffer size, later resize as needed*/
        size_t receive_buf_size = BUF_SIZE;
        char *buf = (char *)malloc(receive_buf_size);
        memset(buf, 0, BUF_SIZE);
        if (buf == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for receiving buffer");
            close(new_fd);
            continue;
        }

        int length;
        size_t total_received = 0, valid_size = 0;
        char *end_packet = NULL;

        /* Loop to receive data until a newline is found */
        do
        {
            static int count = 0;
    
            /* Resize buffer dynamically if needed from the second count*/
            if ((total_received + 1 >= receive_buf_size) && (count != 0))
            {
                /* Double the size */
                receive_buf_size *= 2;
                char *new_buf = (char *)realloc(buf, receive_buf_size);
                if (new_buf == NULL)
                {
                    syslog(LOG_ERR, "Realloc failed for receiving buffer");
                    free(buf);
                    close(new_fd);
                    continue;
                }
                memset(new_buf + total_received, 0, receive_buf_size - total_received);
                buf = new_buf;
            }

            /* Receive data. ssize_t recv(int sockfd, void buf[.len], size_t len, int flags);*/
            length = recv(new_fd, buf + total_received, receive_buf_size - total_received - 1, 0);
            if (length == -1)
            {
                syslog(LOG_ERR, "Receive failed");
                free(buf);
                close(new_fd);
                continue;
            }

            total_received += length;
            end_packet = strchr(buf, '\n');
            count++;

        } while ((end_packet == NULL) && (length > 0));

    /* Check if we found a newline */
        if (end_packet != NULL)
        {
            /* Append to file only until \n */
            valid_size = end_packet - buf + 1; 
            /* Null terminate the string */
            buf[valid_size] = '\0';
            fwrite(buf, sizeof(char), valid_size, tmp_file);
            fflush(tmp_file);
        }
        else
        {
            syslog(LOG_ERR, "Received without newline\n");
        }
            
        free(buf);

        /* Send back to client */
        fseek(tmp_file, 0, SEEK_SET);

        size_t send_buf_size = valid_size;
        char *send_buf = (char *)malloc(send_buf_size);
        if (send_buf == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for sending buffer");
            close(new_fd);
            continue;
        }

        while (fgets(send_buf, send_buf_size, tmp_file) != NULL) 
        {
            if (send(new_fd, send_buf, strlen(send_buf), 0) == -1) 
            {
                syslog(LOG_ERR, "Send to client failed");
                break;
            }
        }

        free(send_buf);
        close(new_fd);
    }

    cleanup();
    closelog();
}