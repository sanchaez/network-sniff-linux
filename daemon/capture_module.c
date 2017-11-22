/*
 * Implementation of the packet capture library for netsniffd
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <net/if.h>
#include <netinet/in.h>

#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "capture_module.h"

/* Log writer */
#include <syslog.h>

/* Red-Black tree from glibc */
/* Note: using radix trees would be much better here */
#define _GNU_SOURCE
#include <search.h>

pthread_t capture_thread;
/* Mutex is used as a thread cancellation flag.
 * It locks upon thread creation and releases when thread should exit.
 * This is done to control a loop inside the thread. */
pthread_mutex_t stop_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Mutex to control access to stats */
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

iface_stat g_stats;

const char *statsfile = "/var/tmp/netsniffd/stats";
const char *default_iface = "eth0";

#define SOCKET_DATA_SIZE_MAX 65536

/***********************************/
/* iface_stat manipulation helpers */
/***********************************/

static int
iface_stat_init(piface_stat stats)
{
    if(stats)
    {
        /* copy and ensure NUL-termination */
        strncpy(stats->iface_str, default_iface, IFNAMSIZ-1);
        stats->iface_str[IFNAMSIZ-1] = '\0';

        stats->ip_stats = NULL;
        stats->entries_count = 0;

        return 0;
    }

    return -1; /* nothing to work with */
}

static int
packet_stats_dump(iface_stat *stats)
{
    return 0;
}

static int
packet_stats_load(iface_stat *stats)
{
    return 0;
}

/*****************/
/* Worker thread */
/*****************/

/*
 * Check for locked mutex.
 * Returns nonzero if the mutex is locked, meaning thread should still run.
 * If mutex was unlocked, returns zero.
 */
static int
is_stopped(pthread_mutex_t *mtx)
{
  switch(pthread_mutex_trylock(mtx))
  {
    case 0: /* if we got the lock, unlock and return zero (false) */
      pthread_mutex_unlock(mtx);
      return 0;
    case EBUSY: /* return nonzero (true) if the mutex was locked */
      return 1;
  }

  return -1; /* FIXME: EINVAL is not handled, thread stops */
}

/* Returns 'struct thread_arg' */
static void*
packet_loop_fn(void* arg)
{
    int data_retrieved_size, err = 0;
    socklen_t saddr_len;
    int *return_value = NULL;
    struct sockaddr saddr;
    //struct in_addr in;
    unsigned char buffer[SOCKET_DATA_SIZE_MAX];

    /* open socket for sniffing */
    int capture_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    if(capture_socket < 0)
    {
        err = errno;
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(err));
        return_value = malloc(sizeof(int));
        *return_value = err;
        return return_value;
    }

    /* configure socket interface */
    setsockopt(capture_socket, SOL_SOCKET, SO_BINDTODEVICE, &(g_stats.iface_str), IFNAMSIZ);

    /* capture packets */
    while(!is_stopped(&stop_mutex))
    {
        saddr_len = sizeof(saddr);
        data_retrieved_size = recvfrom(capture_socket,
                                       (void *)&buffer,
                                       SOCKET_DATA_SIZE_MAX,
                                       0, &saddr, &saddr_len);
        if(data_retrieved_size < 0)
        {
            err = errno;
            syslog(LOG_WARNING, "recvfrom failed: %s", strerror(err));
            continue;
        }

        /* process packet */

    }

    /* cleanup */

    return return_value;
}

/*********************/
/* Library interface */
/*********************/

int
packet_capture_start()
{
    /* Trylock a mutex. If it locks (zero) then thread was stopped.
       Do nothing when mutex was already locked. */
    if(is_stopped(&stop_mutex))
    {
        int err;

        /* load stats */
        /* !!! malloc !!! */
        if(packet_stats_load(&g_stats))
        {
            /* load failed - create new record */
            iface_stat_init(&g_stats);
        }

        /* !!! create thread !!! */
        pthread_mutex_lock(&stop_mutex);
        err = pthread_create(&capture_thread, NULL, &packet_loop_fn, NULL);
        if(err)
        {
            syslog(LOG_ERR, "pthread_create failed: %s", strerror(err));
            return err;
        }
    } /* else - thread already running */

    return 0;
}

int
packet_set_iface(const char *iface_str)
{
    return 0;
}

int
packet_ip_stats(const char *ip_str, pip_stat stats)
{
    return 0;
}

int
packet_iface_stats(const char *iface_str, piface_stat stats)
{
    return 0;
}

int
packet_capture_stop()
{
    /* Trylock a mutex. If it is busy, thread is still running. */
    if(!is_stopped(&stop_mutex))
    {
        int *return_val = NULL;

        /* Unlock mutex used as a cancelation flag and wait for thread to return. */
        /* !!! join thread !!! */
        pthread_mutex_unlock(&stop_mutex);
        pthread_join(capture_thread, (void **)return_val);

        /* check return val */
        if(return_val)
        {
            syslog(LOG_ERR, "Error encountered in thread: %s", strerror(*return_val));
            free(return_val);
        }

        packet_stats_dump(&g_stats);
    }
    return 0;
}

