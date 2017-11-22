/*
 * Header for packet capture library for netsniffd
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#ifndef CAPTURE_MODULE_H
#define CAPTURE_MODULE_H

/* Uses definitions from <net/if.h> */

/**
 * @struct ip_stat
 * @typedef ip_stat, *pip_stat
 */
typedef struct {
    struct in_addr ip;
    int count;
} ip_stat, *pip_stat;

/**
 * @struct iface_stat
 * @typedef iface_stat, *iface_stat
 */
typedef struct {
    char iface_str[IFNAMSIZ];
    void *ip_stats_tree;
    int entries_count;
} iface_stat, *piface_stat;

/**
 * @fn packet_capture_loop
 * @brief
 * @return
 */
int
packet_capture_start();

/**
 * @fn packet_set_iface
 * @brief
 * @param iface_str
 * @return
 */
int
packet_set_iface(const char* iface_str);

/**
 * @fn packet_iface_stats
 * @brief
 * @param iface_str
 * @param stats
 * @return
 */
int
packet_iface_stats(const char* iface_str, piface_stat stats);

/**
 * @fn packet_ip_stats
 * @brief
 * @param ip_str
 * @param stats
 * @return
 */
int
packet_ip_stats(const char* ip_str, pip_stat stats);

/**
 * @fn packet_capture_stop
 * @brief
 * @return
 */
int
packet_capture_stop();

#endif // CAPTURE_MODULE_H
