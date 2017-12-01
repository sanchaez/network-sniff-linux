/*
 * Definitions of types used for IPC
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#ifndef CUSTOM_COM_DEF_H
#define CUSTOM_COM_DEF_H

#define IPC_SOCKET_PATH "/tmp/netsniffd.sock"

/**
 * @enum passed_option
 * @brief Enumerated options for daemon.
 *
 * This enum should be passed as uint32_t value.
 *
 * Commands:
 * DOPT_START       start sniffing on eth0
 * DOPT_STOP        stop sniffing
 * DOPT_CLOSE       stop daemon
 * DOPT_SET_IFACE   set interface for sniffing
 * DOPT_IP_COUNT    request hit count for an IP
 * DOPT_STAT        request stats for the interface or for all interfaces
 *
 * Arguments (in order):
 * DOPT_START       -
 * DOPT_STOP        -
 * DOPT_CLOSE       -
 *
 * DOPT_SET_IFACE   uint32_t              iface_name_size (cannot be 0)
 *                  char[iface_name_size] iface_name
 *
 * DOPT_IP_COUNT    char[INET_ADDRSTRLEN] ip_str
 *
 * DOPT_STAT        uint32_t              iface_name_size (can be 0)
 *                  char[ip_str_size]     iface_name      (can be NULL)
 *
 * All options except DOPT_STOP send int32_t `status_code` first to
 * indicate if they succeeded or failed. If they succeed, reply is
 * sent afterwards if needed. Failure statuses should match errno codes.
 *
 * Reply values:
 * DOPT_START       -
 * DOPT_STOP        -
 * DOPT_SET_IFACE   -
 *
 * DOPT_IP_COUNT    uint32_t    count
 *                  0 means that IP was not found.
 *
 * DOPT_STAT        uint32_t                    iface_count
 *                  uint32_t[iface_count]       stats_count
 *                  char[IFNAMSIZ][iface_count] iface_names
 *
 *                  (for 0 <= i < iface_count) {
 *                      (for 0 <= j < stats_count[i]) {
 *                          char[INET_ADDRSTRLEN] ip_j
 *                          uint32_t              count_j
 *                      }
 *                  }
 *
 * Notes:
 * INET_ADDRSTRLEN is defined in <netinet/in.h>
 * IFNAMSIZ is defined in <net/if.h>
 */
enum passed_option
{
    DOPT_START,
    DOPT_STOP,
    DOPT_SET_IFACE,
    DOPT_STAT,
    DOPT_IP_COUNT
};

/* TODO: maybe send confirmation bit? */

#endif // CUSTOM_COM_DEF_H
