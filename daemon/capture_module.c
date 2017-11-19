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

#include "capture_module.h"

/* Red-Black tree from glibc */
/* Note: using radix trees would be much better here */
#include <search.h>

//static pthread_t capture_thread;
//static iface_stat stats;

int
packet_capture_loop()
{
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
    return 0;
}

void
packet_loop_fn()
{
    while(1)
    {

    }
}

int
packet_capture_init()
{
    return 0;
}

int
packet_stats_dump()
{
    return 0;
}
