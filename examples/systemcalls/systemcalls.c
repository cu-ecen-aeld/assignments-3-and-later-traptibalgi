
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
 * 5. 
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

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    openlog("systemcalls", LOG_PID, LOG_USER);
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
        } else 
        {
            syslog(LOG_ERR, "Command was NULL and shell is not available!");
        }
        return sys_status;
    }

    /* Lines 60 - 67 were partially generated using ChatGPT at https://chat.openai.com/ with prompts including [system() return if command successful with examples] */

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
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }

    command[count] = NULL;
    

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    va_end(args);

    return true;
}
