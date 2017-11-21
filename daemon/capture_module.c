/*
 * Implementation of the packet capture library for netsniffd
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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

static pthread_t capture_thread = -1;
static piface_stat stats;
static int is_running;

const char *statsfile = "/var/tmp/netsniffd/stats";
const char *default_iface = "eth0";

piface_stat
iface_stat_init()
{
    piface_stat new_iface_stat;
    new_iface_stat = malloc(sizeof(iface_stat));
    if(!new_iface_stat)
    {
        errno = ENOMEM; // should not be needed?
        return NULL;
    }

    new_iface_stat->iface_str = default_iface;
    new_iface_stat->entries_count = 0;
    new_iface_stat->ip_stats = NULL;
    return new_iface_stat;
}

int
packet_stats_dump()
{
    return 0;
}

int
packet_stats_load()
{
    return 0;
}

void
iface_stat_delete(piface_stat iface_stat)
{
    if(iface_stat)
    {
        if(iface_stat->ip_stats)
            free(iface_stat->ip_stats);

        free(iface_stat); // FIXME: Not a vector
    }
}


void*
packet_loop_fn(void* arg)
{
    (void)arg;
    while(1)
    {

    }
}

int
packet_capture_init()
{
    /* load stats */
    if(packet_stats_load())
    {
        /* load failed - create new file */
        stats = iface_stat_init();
        if(!stats)
        {
            int err = errno;
            syslog(LOG_ERR, "iface_stat_init failed: %s", strerror(err));
            return err;
        }
    }

    return 0;
}

int
packet_capture_loop()
{
    if(!is_running)
    {
        int err;

        err = packet_capture_init();

        if(err)
        {
            syslog(LOG_ERR, "packet_capture_init failed: %s", strerror(err));
            return err;
        }

        /* create thread */
        err = pthread_create(&capture_thread, NULL, &packet_loop_fn, NULL);
        if(err)
        {
            syslog(LOG_ERR, "pthread_create failed: %s", strerror(err));
            return err;
        }
    }
    return 0; /* thread already running */
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
    pthread_cancel(capture_thread);
    return 0;
}

