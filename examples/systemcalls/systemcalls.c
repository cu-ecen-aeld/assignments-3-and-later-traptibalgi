
/*******************************************************************************
 * Copyright (C) 2024 by Trapti Damodar Balgi
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. Users are
 * permitted to modify this and use it to learn about the field of embedded
 * software. <Student Name> and the University of Colorado are not liable for
 * any misuse of this material.
 * ****************************************************************************/

/**
 * @file    systemcalls.c
 * @brief   Functions to execute system calls
 *
 * @author  Trapti Damodar Balgi
 * @date    09/08/2024
 * @references  
 * 1. https://linux.die.net/man/3/syslog
 * 2. https://man7.org/linux/man-pages/man3/system.3.html
 * 3. AESD Lectures and Slides
 * 4. https://linux.die.net/man/3/execv
 * 5. https://man7.org/linux/man-pages/man2/fork.2.html
 * 6. https://chatgpt.com/ - To learn about concepts
 * 7. Linux System Programming
 * 8. https://stackoverflow.com/a/13784315/1446624
 *
 */

#include "systemcalls.h"
#include <syslog.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    openlog("system_calls", LOG_PID, LOG_USER);
    int ret_status;
    bool sys_status = false;

    /* Check for cmd == NULL.
        system(NULL) checks if the shell is available */

    if (cmd == NULL) 
    {
        ret_status = system(NULL);
        if (ret_status != 0) 
        {
            syslog(LOG_ERR, "Command was NULL but shell is available!\n");
        } 
        else 
        {
            syslog(LOG_ERR, "Command was NULL and shell is not available!");
        }
        closelog();
        return sys_status;
    }

    /* Lines 81 - 87 were partially generated using ChatGPT at https://chat.openai.com/ with prompts including [system() return if command successful with examples] */

    ret_status = system(cmd);

    if (ret_status == -1)
    {
        syslog(LOG_ERR, "Child process could not be created, %s\n", strerror(errno));
    }
    else if (WIFEXITED(ret_status) && WEXITSTATUS(ret_status) == 127)
    {
        syslog(LOG_ERR, "Command could not be executed in child process!\n");
    }
    else if (WIFEXITED(ret_status) && WEXITSTATUS(ret_status) == 0)
    {
        syslog(LOG_INFO, "Command executed successfully with status: %d\n", WEXITSTATUS(ret_status));
        sys_status = true;
    }
    else
    {
        syslog(LOG_ERR, "Command failed with status: %d\n", WEXITSTATUS(ret_status));
    }

    closelog();

    return sys_status;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    openlog("system_calls", LOG_PID, LOG_USER);

    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }

    command[count] = NULL;

    /*The path needs to be absolute so must start with root*/
    if (command[0] == NULL || command[0][0] != '/')
    {
        syslog(LOG_ERR, "Error with command. Command should not be NULL and be an absolute path!\n");
        va_end(args);
        closelog();
        return false;
    }

    /*Create a fork for child process*/

    pid_t pid = fork();

    if (pid == -1)
    {
        syslog(LOG_ERR, "Fork failed with error:, %s\n", strerror(errno));    
        va_end(args);
        closelog();
        return false;
    }
    if (pid == 0)
    {
        /*Fork was successful, call execv*/
        int ret_val;
        ret_val = execv(command[0], command);
        if (ret_val == -1)
        {
            syslog(LOG_ERR, "Exec failed with error:, %s\n", strerror(errno));
            va_end(args);
            closelog();
            exit(1);
        }
    }
    else 
    {
        /*Parent process*/
        int child_status;
        pid_t ret_status;

        ret_status = waitpid(pid, &child_status, 0);

        if (ret_status == -1)
        {
            syslog(LOG_ERR, "Wait for child process failed!\n");
            va_end(args);
            closelog();
            return false;
        }

        /* Check if child process terminated and check exit status*/
        if (WIFEXITED(child_status)) 
        {
            /* If status is 0, exited normally*/
            if (WEXITSTATUS(child_status) == 0)
            {
                syslog(LOG_INFO, "Child process exited normally\n");
                va_end(args);
                closelog();
                return true;
            }
            else
            {
                syslog(LOG_INFO, "Child exited with status %d\n", child_status);
                va_end(args);
                closelog();
                return false;
            }
        } 
        else 
        {
            syslog(LOG_ERR, "Child process did not exit normally\n");
            va_end(args);
            closelog();
            return false;
        }
    }
    va_end(args);
    closelog();
    return false;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    openlog("system_calls", LOG_PID, LOG_USER);

    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;


    /*The path needs to be absolute so must start with root*/
    if (command[0] == NULL || command[0][0] != '/' || outputfile == NULL)
    {
        syslog(LOG_ERR, "Error with command. Command should not be NULL and be an absolute path!\n");
        va_end(args);
        closelog();
        return false;
    }

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) 
    { 
        syslog(LOG_ERR, "Error opening file!\n");
        abort(); 
    }

    /*Create a fork for child process*/
    pid_t pid = fork();

    if (pid == -1)
    {
        syslog(LOG_ERR, "Fork failed with error:, %s\n", strerror(errno));    
        va_end(args);
        closelog();
        return false;
    }
    if (pid == 0)
    {
        /*Redirect output to file*/
        if (dup2(fd, 1) < 0) 
        { 
            syslog(LOG_ERR, "Dup2 failed!\n");
            abort(); 
        }
        close(fd);

        /*Fork was successful, call execv*/
        int ret_val;
        ret_val = execv(command[0], command);
        if (ret_val == -1)
        {
            syslog(LOG_ERR, "Exec failed with error:, %s\n", strerror(errno));
            va_end(args);
            closelog();
            exit(1);
        }
    }
    else 
    {
        /*Parent process*/ 

        close(fd);

        int child_status;
        pid_t ret_status;

        ret_status = waitpid(pid, &child_status, 0);

        if (ret_status == -1)
        {
            syslog(LOG_ERR, "Wait for child process failed!\n");
            va_end(args);
            closelog();
            return false;
        }

        /* Check if child process terminated and check exit status*/
        if (WIFEXITED(child_status)) 
        {
            /* If status is 0, exited normally*/
            if (WEXITSTATUS(child_status) == 0)
            {
                syslog(LOG_INFO, "Child process exited normally\n");
                va_end(args);
                closelog();
                return true;
            }
            else
            {
                syslog(LOG_INFO, "Child exited with status %d\n", child_status);
                va_end(args);
                closelog();
                return false;
            }
        } 
        else 
        {
            syslog(LOG_ERR, "Child process did not exit normally\n");
            va_end(args);
            closelog();
            return false;
        }
    }
    va_end(args);
    closelog();
    return false;
}
