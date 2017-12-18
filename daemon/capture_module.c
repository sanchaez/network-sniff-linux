/*
 * Implementation of the packet capture library for netsniffd
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#include "stdafx.h"

#include <arpa/inet.h>
#include <pthread.h>

/**
 * @struct s_internal_ip_stat
 * @typedef internal_ip_stat
 */
typedef struct s_internal_ip_stat {
    struct in_addr ip;
    long count;
} internal_ip_stat;

/**
 * @struct s_internal_iface_stat
 * @typedef internal_iface_stat
 */
typedef struct s_internal_iface_stat {
    char iface_str[IFNAMSIZ];
    void *ip_stats_tree;
    long entries_count;
} internal_iface_stat;

internal_iface_stat g_stats;

pthread_t capture_thread;

/* Mutex is used as a thread cancellation flag.
 * It locks upon thread creation and releases when thread should exit.
 * This is done to control a loop inside the thread. */
pthread_mutex_t stop_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Mutex to control access to stats */
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

#define STATSFILE_TEMPLATE "/var/tmp/netsniffd/%s.stat"
#define DEFAULT_IFACE "ens33"
#define SOCKET_DATA_SIZE_MAX 65536

char *iface_name = DEFAULT_IFACE;

/***********************************/
/* structure manipulation helpers */
/***********************************/

static int
ip_stat_new(internal_ip_stat ** stat, struct in_addr *ip)
{
    /* !!! malloc !!! */
    *stat = malloc(sizeof(struct s_internal_iface_stat));
    if(!stat)
        return errno;

    (*stat)->ip = *ip;
    (*stat)->count = 1;

    return 0;
}

static void
ip_stat_delete(void *stat)
{
    free((internal_ip_stat *)stat);
}

static int
ip_stat_compare_fn(const void *l, const void *r)
{
    const internal_ip_stat *stat_l = (const internal_ip_stat *) l;
    const internal_ip_stat *stat_r = (const internal_ip_stat *) r;

    if(stat_l->ip.s_addr > stat_r->ip.s_addr)
        return 1;

    if(stat_l->ip.s_addr < stat_r->ip.s_addr)
        return -1;

    return 0;
}

static int
iface_stat_init(internal_iface_stat * stats)
{
    if(!stats)
        return -1; /* nothing to work with */

    /* copy and ensure NUL-termination */
    strncpy(stats->iface_str, DEFAULT_IFACE, IFNAMSIZ-1);
    stats->iface_str[IFNAMSIZ-1] = '\0';

    stats->ip_stats_tree = NULL;
    stats->entries_count = 0;

    return 0;
}

/***************************/
/* Serialization functions */
/***************************/

/* supefluos buffer to store int */
#define MAX_INT_CHARS 256
#define IP_STAT_STRING_BUFSIZ INET_ADDRSTRLEN + MAX_INT_CHARS + 3
const char *entry_pattern = "%s;%d\n";
FILE *fd;

static char *
ipstat2str(internal_ip_stat *stat)
{
    char *result;
    char ip_buffer[INET_ADDRSTRLEN];

    /* convert IP address */
    if(!inet_ntop(AF_INET, &stat->ip, ip_buffer, INET_ADDRSTRLEN))
    {
        return NULL;
    }

    /* !!! malloc !!! */
    result = malloc(IP_STAT_STRING_BUFSIZ);
    if(!result)
        return NULL;
    if(snprintf(result, IP_STAT_STRING_BUFSIZ, entry_pattern, result, stat->count) < 0)
    {
        /* errno is set on POSIX */
        free(result);
        return NULL;
    }

    return result;
}

static void
ip_stat_tree_serialize_fn(const void *nodep, VISIT order, int depth)
{
    /* ignore depth */
    (void)depth;

    /* we are interested in preorder and leaf */
    switch(order)
    {
    case preorder:
    case leaf:
        {
            internal_ip_stat * data = *((internal_ip_stat **) nodep);
            char *string_to_print = ipstat2str(data);
            if(string_to_print)
            {
                fputs(string_to_print, fd);
                /* !!! free !!! */
                free(string_to_print);
            }
        }
        break;

    case postorder:
    case endorder:
        break;
    }
}

static int
packet_stats_dump(internal_iface_stat *stats)
{
    char filename_buffer[FILENAME_MAX];

    if(snprintf(filename_buffer,
                FILENAME_MAX,
                STATSFILE_TEMPLATE,
                stats->iface_str) < 0)
    {
        /* errno is set on POSIX */
        return errno;
    }

    fd = fopen(filename_buffer, "w");
    if(!fd)
    {
        return errno;
    }

    /* Walk the tree and put nodes to the file in defined strings.
       It will use the global fd that we have set up earlier */
    twalk(stats->ip_stats_tree, ip_stat_tree_serialize_fn);

    fclose(fd);
    return 0;
}

static int
packet_stats_load(internal_iface_stat *stats)
{
    char filename[FILENAME_MAX];

    if(snprintf(filename,
                FILENAME_MAX,
                STATSFILE_TEMPLATE,
                stats->iface_str) < 0)
    {
        /* errno is set on POSIX */
        return errno;
    }

    fd = fopen(filename, "r");
    if(!fd)
        return errno;

    char *ip_buffer = NULL, *count_buffer = NULL;
    size_t len_ip = 0, len_count = 0;
    ssize_t read_ip = 0, read_count = 0;
    int err = 0;
    /*
     * Assuming the following format:
     * 255.255.255.255;12345\n
     */
    while((read_ip >= 0) && (read_count >= 0))
    {
        char *endptr;
        internal_ip_stat *new_stat, *tree_stat;
        read_ip = getdelim(&ip_buffer, &len_ip, ';', fd);
        /* getdelim might fail */
        if(errno)
        {
            err = errno;
            syslog(LOG_ERR, "getdelim() failed: %s", strerror(err));
            break;
        }

        read_count = getline(&count_buffer, &len_count, fd);
        /* getline might fail */
        if(errno)
        {
            err = errno;
            syslog(LOG_ERR, "getline() failed: %s", strerror(err));
            break;
        }

        new_stat = malloc(sizeof(new_stat));
        if(!new_stat)
        {
            syslog(LOG_ERR, "malloc() failed: %s", strerror(ENOMEM));
            fclose(fd);
            return ENOMEM;
        }

        /* a line is read, converting */
        /* convert IP */
        if(!inet_pton(AF_INET, ip_buffer, &new_stat->ip))
            continue;

        /* convert count */
        new_stat->count = strtol(count_buffer, &endptr, 10);
        if(errno)
        {
            err = errno;
            syslog(LOG_ERR, "strtol() failed: %s", strerror(err));
            continue;
        }
        if(endptr == count_buffer)
        {
            syslog(LOG_ERR, "strtol() failed: No digits were found (%s)", endptr);
            continue;
        }

        /* add new_stat to tree */
        tree_stat = tsearch((void *) new_stat,
                            g_stats.ip_stats_tree,
                            ip_stat_compare_fn);
        if(tree_stat != new_stat)
        {
            /* in this case value was found and overwritten */
            free(new_stat);
        }
    }

    fclose(fd);
    return err;
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
is_running(pthread_mutex_t *mtx)
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
work_with_addr(struct in_addr *addr, internal_iface_stat *stat)
{
    internal_ip_stat *new_ip_stat = NULL, *found_stat;
    void *returned_value;
    int err;

    syslog(LOG_DEBUG, "work: creating new struct");
    err = ip_stat_new(&new_ip_stat, addr);
    if(err)
        return err;

    syslog(LOG_DEBUG, "work: adding to tree");
    returned_value = tsearch((void *) new_ip_stat, &stat->ip_stats_tree, &ip_stat_compare_fn);
    if(!returned_value)
    {
        /* tsearch fails when no element can be allocated */
        return ENOMEM;
    }

    found_stat = (*(internal_ip_stat **)returned_value);

    if(found_stat != new_ip_stat)
    {
        /* increment count if a value was found */
        ++found_stat->count;
        free(new_ip_stat);
    }
    else
    {
        /* new entry was added, increment entries_count */
        ++stat->entries_count;
    }

    return 0;
}

/* thread errors */
int thread_last_error;

/* Returns NULL */
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
    pthread_mutex_lock(&stats_mutex);
    setsockopt(capture_socket,
               SOL_SOCKET,
               SO_BINDTODEVICE,
               &(g_stats.iface_str),
               IFNAMSIZ);
    pthread_mutex_unlock(&stats_mutex);

    syslog(LOG_DEBUG, "start capture: %s", g_stats.iface_str);
    /* capture packets */
    while(is_running(&stop_mutex))
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
        } else {
            char addr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &saddr.sin_addr, addr, INET_ADDRSTRLEN);
            syslog(LOG_DEBUG, "recvfrom succeeded: %s", addr);
        }

        /* process packet */
        /* we only need to analyze sockaddr_in structure here to retrieve IP */
        pthread_mutex_lock(&stats_mutex);
        thread_last_error = work_with_addr(&saddr.sin_addr, &g_stats);
        pthread_mutex_unlock(&stats_mutex);
        if(thread_last_error)
        {
            syslog(LOG_ERR, "work_with_addr failed: %s", strerror(thread_last_error));
            return NULL;
        }
    }
    syslog(LOG_DEBUG, "stop capture: %s", g_stats.iface_str);
    return NULL;
}

/*********************/
/* Library interface */
/*********************/

int
packet_capture_start()
{
    int err;

    syslog(LOG_DEBUG, "start capture");
    /* Trylock a mutex. If it locks then thread was stopped.
       Do nothing when mutex was already locked. */
    if(is_running(&stop_mutex))
    {
        syslog(LOG_DEBUG, "mutex locked");
        return 0;
    }

    /* load stats */
    if(packet_stats_load(&g_stats))
    {
        /* load failed - create new record */
        syslog(LOG_DEBUG, "previous stats not loaded");
        iface_stat_init(&g_stats);
    } else {
        syslog(LOG_DEBUG, "previous stats loaded");
    }

    /* !!! create thread !!! */
    pthread_mutex_lock(&stop_mutex);
    err = pthread_create(&capture_thread, NULL, &packet_loop_fn, NULL);
    if(err)
    {
        syslog(LOG_ERR, "pthread_create failed: %s", strerror(err));
        return err;
    }

    return 0;
}

int
packet_set_iface(const char *iface_str)
{
    return 0;
}

int packet_get_iface_stats(packet_interface_stats **stats_out,
                           size_t *stats_size_out,
                           const char *iface_str)
{
    return 0;
}

int packet_get_ip_stats(const char *ip_str, packet_ip_stats *stats)
{
    return 0;
}

int packet_get_ip_count(const char *ip_str)
{
    internal_ip_stat search_stats, *result_stats;

    if(inet_pton(AF_INET, ip_str, &search_stats.ip) == -1)
    {
        int err = errno;
        syslog(LOG_ERR, "inet_pton: %s", strerror(err));
        return 0;
    }

    result_stats = tfind(&search_stats, &g_stats.ip_stats_tree, ip_stat_compare_fn);
    if(!result_stats) {
        return 0;
    }

    return result_stats->count;
}

int
packet_capture_stop()
{
    /* Trylock a mutex. If it is busy, thread is likely still running. */
    if(!is_running(&stop_mutex))
            return 0;

    /* Unlock mutex used as a cancelation flag and wait for thread to return. */
    /* !!! join thread !!! */
    pthread_mutex_unlock(&stop_mutex);
    pthread_join(capture_thread, NULL);

    /* check last error*/
    if(thread_last_error)
    {
        syslog(LOG_ERR, "Error encountered in thread: %s", strerror(thread_last_error));
        return thread_last_error;
    }

    packet_stats_dump(&g_stats);

    return 0;
}

void packet_stats_clear()
{
    tdestroy(g_stats.ip_stats_tree, ip_stat_delete);
}

