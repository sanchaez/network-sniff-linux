/*
 * netsniffd - Simple Linux network sniffer daemon
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#include "stdafx.h"

/* umask() */
#include <sys/types.h>
#include <sys/stat.h>

/* Signal handling */
#include <signal.h>

/* UNIX sockets */
#include <sys/un.h>

/* Shared definition of a struct used to communicate */
#include "custom_com_def.h"

#define CONN_MAX 5

int ipc_socket_fd;

/**
 * @fn daemonize
 * @brief Convert this process to a daemon.
 */
void
daemonize(void)
{
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* If we got a good PID, then
        we can exit the parent process. */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Move on as a child process!          */
    /* We can do whatever we want from here */

    /* Set default file permissions to 0000 */
    umask(0);

    /* Open syslog */
    openlog("netsniffd", LOG_PID, LOG_DAEMON);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0)
    {
        const int err = errno;
        syslog(LOG_ERR, "setsid:%s", strerror(err));
        exit(EXIT_FAILURE);
    }

    /* set file descriptors to /dev/null to ignore any reading/writing
       this is better than just closing */
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

/**
 * @fn sigterm_handler
 * @brief Handler of SIGTERM
 * @param signum value ignored.
 */
__attribute__((noreturn))
void
sigterm_handler(int signum)
{
    (void)signum;
    syslog(LOG_DEBUG, "Exiting...");
    close(ipc_socket_fd);
    packet_capture_stop();
    exit(EXIT_SUCCESS);
}

/* Custom wrappers */

int
recv_logged(int sock, void *buffer, int size)
{
    int err;
    ssize_t n = recv(sock, (void*) buffer, size, 0);
    if(n <= 0)
    {
        if (n < 0)
        {
            err = errno;
            syslog(LOG_ERR, "recv() failed: %s", strerror(err));
            return err;
        }
        else
        {
            syslog(LOG_WARNING, "recv(): Empty message recieved");
        }
    }

    return 0;
}

int
send_logged(int sock, void *buffer, int size)
{
    int err;
    ssize_t n = send(sock, buffer, size, 0);
    if(n == -1)
    {
        err = errno;
        syslog(LOG_ERR, "send() failed: %s", strerror(err));
        return err;
    }

    return 0;
}

int
read_str_arg(int socket_fd, char **str)
{
    int err;
    uint32_t str_size;
    err = recv_logged(socket_fd, &str_size, sizeof(str_size));
    if(err)
        return err;

    if(!str_size)
    {
        *str = NULL;
        return 0;
    }

    *str = malloc(str_size);
    if(!*str)
    {
        err = errno;
        syslog(LOG_ERR, "malloc() failed: %s", strerror(err));
        return err;
    }

    err = recv_logged(socket_fd, *str, str_size); // FIXME: test buffer size
    if(err)
    {
        free(str);
        return err;
    }

    return 0;
}

/*****************/
/* DOPT handlers */
/*****************/

int
dopt_start_handler(int remote_connection_socket)
{
    int err;
    int32_t reply_status = packet_capture_start();

    err = send_logged(remote_connection_socket, &reply_status, sizeof(reply_status));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_START reply failed!");
        return err;
    }

    return 0;
}

int
dopt_stop_handler(int remote_connection_socket)
{
    int err;
    int32_t reply_status = packet_capture_stop();

    err = send_logged(remote_connection_socket, &reply_status, sizeof(reply_status));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_STOP reply failed!");
        return err;
    }

    return 0;
}

int
dopt_set_iface_handler(int remote_connection_socket)
{
    int32_t reply_status;
    char *arg = NULL;
    int err;

    err = read_str_arg(remote_connection_socket, &arg);
    if(err || !arg)
    {
        syslog(LOG_ERR, "DOPT_SET_IFACE arg not received!");
        if(arg)
            free(arg);

        return err;
    }

    reply_status = packet_set_iface(arg);
    free(arg);

    err = send_logged(remote_connection_socket, &reply_status, sizeof(reply_status));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_SET_IFACE reply failed!");
        return err;
    }

    return 0;
}

int
dopt_ip_count_handler(int remote_connection_socket)
{
    int32_t reply_status;
    uint32_t reply_value;
    char *arg = NULL;
    int err, count;

    /* read arg */
    err = read_str_arg(remote_connection_socket, &arg);
    if(err || !arg)
    {
        syslog(LOG_ERR, "DOPT_IP_COUNT arg not received!");
        return err;
    }

    count = packet_get_ip_count(arg);
    if(count < 0)
    {
        /* this error will be sent back */
        reply_status = errno;
        syslog(LOG_ERR, "DOPT_IP_COUNT: error occured on get_ip_stats: %s",
               strerror(reply_status));
    }

    free(arg);

    /* Send status */
    err = send_logged(remote_connection_socket, &reply_status, sizeof(reply_status));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_STOP status reply failed!");
        return err;
    }

    /* Skip sending args if the status is nonzero */
    if(reply_status)
        return 0;

    reply_value = count;
    syslog(LOG_DEBUG, "DOPT_IP_COUNT: sending value: %d",
           reply_value);

    /* Send value back */
    err = send_logged(remote_connection_socket, &reply_value, sizeof(reply_value));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_STOP value reply failed!");
        return err;
    }

    return 0;
}

int
dopt_stat_handler(int remote_connection_socket)
{
    int32_t reply_status;
    uint32_t iface_stats_size;
    char *arg = NULL;
    int err;
    packet_interface_stats *iface_stats = NULL;
    uint32_t *stats_count, stats_count_size;

    /* read arg */
    err = read_str_arg(remote_connection_socket, &arg);
    if(err) /* e don't care if arg is NULL here */
    {
        syslog(LOG_ERR, "DOPT_IP_COUNT arg not received!");
        return err;
    }

    err = packet_get_iface_stats(&iface_stats, (size_t *) &iface_stats_size, arg);
    if(err)
    {
        /* this error will be sent back */
        reply_status = err;
        syslog(LOG_ERR, "DOPT_IP_COUNT: error occured on get_iface_stats: %s",
               strerror(reply_status));
    }

    /* pre-alloc stats_count to catch and report errors */
    if(!reply_status && !iface_stats_size)
    {
        stats_count_size = sizeof(*stats_count) * iface_stats_size;
        stats_count = malloc(stats_count_size);
        if(!stats_count)
        {
            reply_status = errno;
            syslog(LOG_ERR, "DOPT_IP_COUNT: malloc() failed: %s",
                   strerror(reply_status));
        }
    }

    /* Send status */
    err = send_logged(remote_connection_socket, &reply_status, sizeof(reply_status));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_IP_COUNT status reply failed!");
        return err;
    }

    /* Skip sending args if the status is nonzero */
    if(reply_status)
        return 0;

    /* Send iface_count */
    err = send_logged(remote_connection_socket, &iface_stats_size, sizeof(iface_stats_size));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_IP_COUNT value reply failed!");
        if(stats_count)
            free(stats_count);
        return err;
    }

    /* Skip other data. 0 here means no data was found.*/
    if(iface_stats_size)
        return 0;

    /* Send stats_count */
    for(int i = 0; i < iface_stats_size; ++i)
    {
        stats_count[i] = iface_stats[i].size;
    }

    err = send_logged(remote_connection_socket, &stats_count, sizeof(iface_stats_size));
    if(err)
    {
        syslog(LOG_ERR, "DOPT_IP_COUNT value reply failed!");
        free(stats_count);
        return err;
    }

    /* Send iface_stats */
    for(int i = 0; i < iface_stats_size; ++i)
    {
        for(int j = 0; j < stats_count[i]; ++j)
        {
            err = send_logged(remote_connection_socket,
                              &iface_stats[i].stats[i].ip,
                              INET_ADDRSTRLEN * sizeof(char));
            if(err)
            {
                syslog(LOG_ERR, "DOPT_IP_COUNT value reply failed!");
                free(stats_count);
                return err;
            }

            err = send_logged(remote_connection_socket,
                              &iface_stats[i].stats[i].count,
                              sizeof(uint32_t));
            if(err)
            {
                syslog(LOG_ERR, "DOPT_IP_COUNT value reply failed!");
                free(stats_count);
                return err;
            }
        }
    }

    free(stats_count);
    return 0;
}

int 
main(void)
{
    struct sockaddr_un local_addr, remote_addr;
    int remote_connection_socket;

    daemonize();
    syslog(LOG_DEBUG, "Process running");

    /* init signal handling */
    signal(SIGTERM, sigterm_handler);

    /* create IPC socket */
    ipc_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(ipc_socket_fd == -1)
    {
        syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
        return 1;
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sun_family = AF_UNIX;
    strncpy(local_addr.sun_path, IPC_SOCKET_PATH, sizeof(local_addr.sun_path)-1);
    unlink(local_addr.sun_path);

    if(bind(ipc_socket_fd, (struct sockaddr*) &local_addr, sizeof(local_addr)) == -1)
    {
        syslog(LOG_ERR, "bind() failed: %s", strerror(errno));
        return 1;
    }

    /* listen */
    if(listen(ipc_socket_fd, 5) == -1)
    {
        syslog(LOG_ERR, "listen() failed: %s", strerror(errno));
        return 1;
    }

    /* main loop */
    for(;;)
    {
        int err;
        uint32_t option;
        socklen_t remote_size = sizeof(remote_addr);

        /* waiting for connection */
        syslog(LOG_DEBUG, "Waiting for connection");
        remote_connection_socket = accept(ipc_socket_fd,
                               (struct sockaddr *) &remote_addr,
                               &remote_size);
        if (remote_connection_socket == -1)
        {
           syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
           return EXIT_FAILURE;
        }
        syslog(LOG_DEBUG, "Connected!");
        /* Now we are connected, read stream and reply */

        /*
        * Daemon now expects this connection to send a uint32_t
        * with one of the passed_option values. It will expect
        * more values depending on an option. They are defined
        * in `shared/custom_com_def.h`.
        */

        /* reading passed_option */
        err = recv_logged(remote_connection_socket, &option, sizeof(option));
        if(err)
        {
            close(remote_connection_socket);
            return EXIT_FAILURE;
        }

        /* take action */

        /* We must send a status value back as well as other
         * defined replies. */
        switch (option)
        {
        case DOPT_START:
            syslog(LOG_DEBUG, "DOPT_START");
            if(dopt_start_handler(remote_connection_socket))
            {
                close(remote_connection_socket);
                return EXIT_FAILURE;
            }
            break;

        case DOPT_STOP:
            syslog(LOG_DEBUG, "DOPT_STOP");
            if(dopt_stop_handler(remote_connection_socket))
            {
                close(remote_connection_socket);
                return EXIT_FAILURE;
            }
            break;

        case DOPT_SET_IFACE:
            syslog(LOG_DEBUG, "DOPT_SET_IFACE");
            if(dopt_set_iface_handler(remote_connection_socket))
            {
                close(remote_connection_socket);
                return EXIT_FAILURE;
            }
            break;

        case DOPT_IP_COUNT:
            syslog(LOG_DEBUG, "DOPT_IP_COUNT");
            if(dopt_ip_count_handler(remote_connection_socket))
            {
                close(remote_connection_socket);
                return EXIT_FAILURE;
            }
            break;

        case DOPT_STAT:
            syslog(LOG_DEBUG, "DOPT_STAT");
            if(dopt_stat_handler(remote_connection_socket))
            {
                close(remote_connection_socket);
                return EXIT_FAILURE;
            }
            break;

        default:
           syslog(LOG_ERR, "Invalid option received!");
        }

        syslog(LOG_DEBUG, "Options parsed.");
        close(remote_connection_socket);
    }
}
