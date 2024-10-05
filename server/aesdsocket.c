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
#define MAX_BUF_SIZE (655536)

int sockfd = -1, new_fd = -1;
struct addrinfo *res;  // will point to the results
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
    if (signal_number == SIGINT)
    {
        syslog(LOG_ERR, "Caught SIGINT, exiting");
        cleanup();
        exit(0);
    }
    else if (signal_number == SIGTERM)
    {
        syslog(LOG_ERR, "Caught SIGTERM, exiting");
        cleanup();
        exit(0);
    }
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

    tmp_file = fopen("/var/tmp/aesdsocketdata", "w+");
    if (tmp_file == NULL) 
    {
        syslog(LOG_ERR, "Failed to open /var/tmp/aesdsocketdata");
        cleanup();
        exit(1);
    }

    /* Now accept an incoming connection in a loop */
    while (1)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Accept failed");
            continue;
        }
    
        char client_ip[INET_ADDRSTRLEN];        /* Size for IPv4 addresses */
        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in*)&their_addr)->sin_addr), client_ip, sizeof(client_ip));
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        char buf[MAX_BUF_SIZE];
        /* Receive data */
        int length = recv(new_fd, buf, sizeof(buf) - 1, 0);
        if (length == -1) 
        {
            syslog(LOG_ERR, "Receive failed");
            close(new_fd);
            continue;
        }
        buf[length] = '\0';

        printf("Received %s\n", buf);

        fprintf(tmp_file, "%s", buf);
        fflush(tmp_file);

        /* Send back to client */
        rewind(tmp_file);

        char send_buf[MAX_BUF_SIZE];
        while (fgets(send_buf, sizeof(send_buf), tmp_file) != NULL) 
        {
            if (send(new_fd, send_buf, strlen(send_buf), 0) == -1) 
            {
                syslog(LOG_ERR, "Send to client failed");
                break;
            }
        }

        printf("Sent %s\n", send_buf);

        if (new_fd != -1) 
        {
            close(new_fd);
        }
    }

    cleanup();
    return 0;
}