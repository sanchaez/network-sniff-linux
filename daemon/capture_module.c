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
#include <netinet/ip.h>

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
/* structure manipulation helpers */
/***********************************/

static int
ip_stat_new(pip_stat stat, struct in_addr *ip)
{
    /* !!! malloc !!! */
    stat = malloc(sizeof(ip_stat));
    if(!stat)
        return ENOMEM;

    stat->ip = *ip;
    stat->count = 1;

    return 0;
}

static int
ip_stat_compare_fn(const void *l, const void *r)
{
    const pip_stat stat_l = (const pip_stat) l;
    const pip_stat stat_r = (const pip_stat) r;

    if(stat_l->ip.s_addr > stat_r->ip.s_addr)
        return 1;

    if(stat_l->ip.s_addr < stat_r->ip.s_addr)
        return -1;

    return 0;
}

static int
iface_stat_init(piface_stat stats)
{
    if(!stats)
        return -1; /* nothing to work with */

    /* copy and ensure NUL-termination */
    strncpy(stats->iface_str, default_iface, IFNAMSIZ-1);
    stats->iface_str[IFNAMSIZ-1] = '\0';

    stats->ip_stats_tree = NULL;
    stats->entries_count = 0;

    return 0;
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

int
work_with_addr(struct in_addr *addr, void **search_tree_root)
{
    pip_stat new_stat = NULL, found_stat;
    void *returned_value;
    int err;

    err = ip_stat_new(new_stat, addr);
    if(err)
        return err;

    returned_value = tsearch((void *) new_stat, search_tree_root, &ip_stat_compare_fn);
    if(!returned_value)
    {
        return ENOMEM;
    }

    found_stat = (*(pip_stat *)returned_value);

    /* increment count if a value was found */
    if(found_stat != new_stat)
    {
        free(new_stat);
        ++found_stat->count;
    } /* else new entry was added, skipping */

    return 0;
}

/* thread errors */
int thread_last_error;

/* Returns 'struct thread_arg' */
static void *
packet_loop_fn(void *arg)
{
    int data_retrieved_size;
    socklen_t saddr_len;
    struct sockaddr_in saddr;
    unsigned char buffer[SOCKET_DATA_SIZE_MAX];

    /* ignore arg */
    (void) arg;

    thread_last_error = 0;

    /* open socket for sniffing */
    int capture_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    if(capture_socket < 0)
    {
        thread_last_error = errno;
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(thread_last_error));

        return NULL;
    }

    /* configure socket interface */
    setsockopt(capture_socket,
               SOL_SOCKET,
               SO_BINDTODEVICE,
               &(g_stats.iface_str),
               IFNAMSIZ);

    /* capture packets */
    while(!is_stopped(&stop_mutex))
    {
        saddr_len = sizeof(saddr);
        data_retrieved_size = recvfrom(capture_socket,
                                       (void *)&buffer,
                                       SOCKET_DATA_SIZE_MAX, 0,
                                       (struct sockaddr *)&saddr,
                                       &saddr_len);
        if(data_retrieved_size < 0)
        {
            thread_last_error = errno;
            syslog(LOG_WARNING, "recvfrom failed: %s", strerror(thread_last_error));
            continue;
        }

        /* process packet */
        /* we only need to analyze sockaddr_in structure here to retrieve IP */
        thread_last_error = work_with_addr(&saddr.sin_addr, &g_stats.ip_stats_tree);\
        if(thread_last_error)
        {
            syslog(LOG_ERR, "work_with_addr failed: %s", strerror(thread_last_error));
            return NULL;
        }
    }

    return NULL;
}

/*********************/
/* Library interface */
/*********************/

int
packet_capture_start()
{
    /* Trylock a mutex. If it locks then thread was stopped.
       Do nothing when mutex was already locked. */
    if(is_stopped(&stop_mutex))
    {
        int err;

        /* load stats */
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
    /* Trylock a mutex. If it is busy, thread is likely still running. */
    if(!is_stopped(&stop_mutex))
    {
        //int *return_val = NULL;

        /* Unlock mutex used as a cancelation flag and wait for thread to return. */
        /* !!! join thread !!! */
        pthread_mutex_unlock(&stop_mutex);
        pthread_join(capture_thread, NULL);

        /* check last error*/
        if(thread_last_error)
        {
            syslog(LOG_ERR, "Error encountered in thread: %s", strerror(thread_last_error));
        }

        packet_stats_dump(&g_stats);
    } /* else - thread already stopped */

    return 0;
}
