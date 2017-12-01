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

typedef struct s_ip_stats
{
    char ip[INET_ADDRSTRLEN];
    uint32_t count;
} packet_ip_stats;

typedef struct s_iface_stats
{
    char ifname[IFNAMSIZ];
    packet_ip_stats *stats;
    size_t size;
} packet_interface_stats;

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
 * @fn packet_get_iface_stats
 * @brief Get stats for interface (or all of them).
 *
 * @param stats         A pointer to dynamically allocated
 *                      packet_interface_stats struct/array.
 * @param stats_size    Size of stats array.
 * @param iface_str     Pointer to interface name.
 *
 * @return 0 on success or an error code on failure.
 *
 * If iface_str is provided, then stats_out will point to a new
 * packet_interface_stats, and *stats_size_out will be set to 1.
 *
 * If iface was not found, stats_out will be NULL and *stats_size_out will be 0.
 *
 * If iface_str is NULL, stats_out will point to an array of packet_interface_stats, and
 * it's size will be written to stats_size_out.
 */

int
packet_get_iface_stats(packet_interface_stats **stats_out, size_t *stats_size_out, const char* iface_str);

/**
 * @fn packet_ip_stats
 * @brief
 * @param ip_str
 * @param stats
 * @return
 */
int
packet_get_ip_stats(const char* ip_str, packet_ip_stats *stats);

/**
 * @brief packet_get_ip_count
 * @param ip_str
 * @return packet count if the entry was found, 0 if not found, -1 if error.
 *
 * When the error occurs, errno is set and -1 is returned.
 */
int
packet_get_ip_count(const char* ip_str);

/**
 * @fn packet_capture_stop
 * @brief
 * @return
 */
int
packet_capture_stop();

/**
 * @fn packet_stats_clear
 * @brief
 * @return
 */
void
packet_stats_clear();

#endif // CAPTURE_MODULE_H
