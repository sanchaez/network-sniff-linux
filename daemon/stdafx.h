/*
 * PCH header for daemon
 *
 * Copyright (c) 2017 Alexander Shaposhnikov <sanchaez@hotmail.com>
 *
 * SPDX-License-Identifier: MIT
 * License-Filename: LICENSE
 */

#ifndef STDAFX_H
#define STDAFX_H

#define _GNU_SOURCE
#define __USE_GNU 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

/* Log writer */
#include <syslog.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>

/* Red-Black tree from glibc */
/* Note: using radix trees would be much better here */
#include <search.h>

#include "capture_module.h"

#endif // STDAFX_H
