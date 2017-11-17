/*
 * netsniffd - Simple Linux network sniffer daemon
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

/* For Linux-specific functions */
#define _GNU_SOURCE


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

/* Log writer */
#include <syslog.h>

/* umask() */
#include <sys/types.h>
#include <sys/stat.h>

/* Signal handling */
#include <signal.h>

void
daemonize(void)
{
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
        we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    /* Move on as a child process!          */
    /* We can do whatever we want from here */

    /* Set default file permissions to 0000 */
    umask(0);

    /* Open syslog */
    openlog("netsniffd", LOG_PID, LOG_DAEMON);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "setsid:%s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* set file descriptors to /dev/null to ignore any reading/writing
       this is better than just closing */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
}

/* Handler for SIGTERM */
void
sigterm_handler(int signum) {
    exit(EXIT_SUCCESS);
}

int 
main(void)
{
    daemonize();
    syslog(LOG_DEBUG, "Process running");

    /* init signal handling */
    signal(SIGTERM, sigterm_handler);

    /* main loop */
    while(1) {
        /* do useful stuff */
        sleep(100);
    }

    exit(EXIT_SUCCESS);
}
