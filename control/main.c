/*
 * CLI control client app for netsniffd
 * 
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 * 
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <netinet/in.h>

#include "custom_com_def.h"


const char *program_name = "netsniff";
const char *program_version = "1.0";
const char *program_author = "Alexander Shaposhnikov <sanchaez@hotmail.com>";

/***********************************/
/* Documentation related functions */
/***********************************/

/**
 * @fn doc_about
 * @brief Print name and authorship message.
 *
 * Prints name and authorship mnessage to the stdout.
 * Uses program_name, program_version and program_author constants.
 */
void
doc_about()
{
    printf("%s: CLI control app for netsniffd version %s.\nCreated by %s.\n",
           program_name, program_version, program_author);
}

/**
 * @fn doc_help
 * @brief Prints help message.
 *
 * Prints help message to the stdout.
 */
void
doc_help(void)
{
    printf("Supported commands:\n");
    printf("--help                  :   print this message.\n");
    printf("--about                 :   print info about the application.\n");
    printf("start                   :   start sniffing packets on a default interface.\n");
    printf("stop                    :   stop sniffing.\n");
    printf("show [ip] count         :   print information about the IP.\n");
    printf("select iface   [iface]  :   select interface for sniffing.\n");
    printf("stat [iface]            :   show statistics for a particular interface.\n");
}

/**
 * @fn doc_usage
 * @brief Prints usage message.
 *
 * Prints usage message to stdout.
 */
void
doc_usage(void)
{
    printf("Usage: %s [OPTIONS...]\n", program_name);
    printf("Use --help for details.\n");
}

/****************************/
/* Daemon control functions */
/****************************/

#define SOCKET_INIT()\
    int ipc_socket;\
    struct sockaddr_un remote;\
    ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);\
    if(ipc_socket == -1)\
    {\
        perror("socket");\
        exit(1);\
    }\
    remote.sun_family = AF_UNIX;\
    strcpy(remote.sun_path, IPC_SOCKET_PATH);\
    if (connect(ipc_socket, (struct sockaddr *) &remote, sizeof(remote)) == -1) {\
        perror("connect");\
        exit(1);\
    }


#define SOCKET_CLEANUP() close(ipc_socket);

/**
 * @fn daemon_start
 * @brief Start netsniffd.
 * @return 0 on success, errno code on failure.
 *
 * Start sniffing packets on a default interface (eth0).
 * Used as a handler to command line parameter.
 */
void
daemon_start(void)
{
    SOCKET_INIT();
    /* send command */
    uint32_t command = DOPT_START, status;
    if (send(ipc_socket, &command, sizeof(command), 0) == -1)
    {
        perror("send");
        SOCKET_CLEANUP();
        exit(1);
    }
    /* receive response */
    if (recv(ipc_socket, &status, sizeof(status), 0) == -1)
    {
        perror("recv");
        SOCKET_CLEANUP();
        exit(1);
    }

    if(status)
    {
        printf("Error occured on netstiffd: %s\n", strerror(status));
    }
    SOCKET_CLEANUP();
}

/**
 * @fn daemon_stop
 * @brief Stop netsniffd.
 * @return 0 on success, errno code on failure.
 *
 * Used as a handler to command line parameter.
 */
void
daemon_stop(void)
{
    SOCKET_INIT();
    /* send command */
    uint32_t command = DOPT_STOP, status;
    if (send(ipc_socket, &command, sizeof(command), 0) == -1)
    {
        perror("send");
        SOCKET_CLEANUP();
        exit(1);
    }
    /* receive response */
    if (recv(ipc_socket, &status, sizeof(status), 0) == -1)
    {
        perror("recv");
        SOCKET_CLEANUP();
        exit(1);
    }

    if(status)
    {
        printf("Error occured on netstiffd: %s\n", strerror(status));
    }
    SOCKET_CLEANUP();
}

/**
 * @fn daemon_print_ip
 * @brief Print packet count for a single IP.
 * @param ip_addr ip address string, cannot be NULL.
 * @return 0 on success, errno code on failure.
 *
 * Prints packet count for a single IP, if it was registered previously.
 * Used as a handler to command line parameter.
 */
void
daemon_print_ip(const char *ip_str)
{
    SOCKET_INIT()
    /* send command */
    uint32_t command = DOPT_IP_COUNT, status, count;
    if (send(ipc_socket, &command, sizeof(command), 0) == -1)
    {
        perror("send");
        SOCKET_CLEANUP();
        exit(1);
    }
    /* send arg */
    char *arg = calloc(INET_ADDRSTRLEN * sizeof(char), 1);
    if(!arg)
    {
        perror("calloc");
        SOCKET_CLEANUP();
        exit(1);
    }

    strcpy(arg, ip_str);
    if (send(ipc_socket, &arg, INET_ADDRSTRLEN * sizeof(char), 0) == -1)
    {
        perror("send");
        SOCKET_CLEANUP();
        exit(1);
    }

    /* receive response */
    if (recv(ipc_socket, &status, sizeof(status), 0) == -1)
    {
        perror("recv");
        SOCKET_CLEANUP();
        exit(1);
    }

    if (recv(ipc_socket, &count, sizeof(count), 0) == -1)
    {
        perror("recv");
        SOCKET_CLEANUP();
        exit(1);
    }
    /* print response */
    printf("%d packets passed thru\n", count);

    if(status)
    {
        printf("Error occured on netstiffd: %s\n", strerror(status));
    }
    SOCKET_CLEANUP()
}

/**
 * @fn daemon_select_iface
 * @brief Select interface to sniff by a daemon.
 * @param iface_str interface name, cannot be NULL.
 * @return 0 on success, errno code on failure.
 *
 * Used as a handler to command line parameter.
 */
void
daemon_select_iface(const char *iface_str)
{
 //   SOCKET_INIT()

 //   SOCKET_CLEANUP()
}

/**
 * @fn daemon_stat
 * @brief Show statistics for a particular interface.
 * @param iface_str interface name, optional.
 * @return 0 on success, errno code on failure.
 *
 * Used as a handler to command line parameter.
 */
void
daemon_stat(const char *iface_str)
{
//    SOCKET_INIT()

 //   SOCKET_CLEANUP()
}

int 
main(int argc, char **argv)
{
    /* Handle command line arguments.
     * FIXME: I would prefer to use argp, but it does not allow
     *        parameters without dashes. */

    /* if no option is given or it's invalid, show usage */
    if (argc < 2 || argc > 4)
    {
        doc_usage();
        return 0;
    }

    /* parse individual options, skipping the app name */
    if(argc == 4 && !strcmp(argv[1], "show") && !strcmp(argv[3], "count"))
    {
        /* handling `show [ip] count` */
        daemon_print_ip(argv[2]);
    }
    else if(argc == 4 && !strcmp(argv[1], "select") && !strcmp(argv[2], "iface"))
    {
        /* handling `select iface [iface]` */
        daemon_select_iface(argv[3]);
    }
    else if(!strcmp(argv[1], "--help"))
    {
        doc_help();
    }
    else if(!strcmp(argv[1], "--about"))
    {
        doc_about();
    }
    else if(!strcmp(argv[1], "start"))
    {
        daemon_start();
    }
    else if(!strcmp(argv[1], "stop"))
    {
        daemon_stop();
    }
    else if(!strcmp(argv[1], "stat"))
    {
        /* check for optional parameter */
        if (argc == 3)
        {
            /* parameter is present */
            daemon_stat(argv[2]);
        }
        else if(argc == 2)
        {
            /* parameter is absent */
            daemon_stat(NULL);
        }
        else /* too many parameters */
            doc_usage();
    }
    else  /* invalid option given, show usage */
        doc_usage();

    return 0;
}
