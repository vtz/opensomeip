/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal lwipopts.h for compiling opensomeip against lwIP headers without a
 * full BSP.  A real product MUST provide its own lwipopts.h tuned for its
 * hardware and memory constraints.
 ********************************************************************************/

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define LWIP_SOCKET          1
#define LWIP_COMPAT_SOCKETS  0
#define LWIP_DNS             1
#define LWIP_NETDB           1
#define LWIP_PROVIDE_ERRNO   1
#define LWIP_TCP             1
#define LWIP_UDP             1
#define LWIP_IPV4            1
#define LWIP_IPV6            0
#define LWIP_IGMP            1

/* Pool sizes below are minimal for CI compilation only.
 * TcpTransportConfig::max_connections defaults to 10, which requires
 * at least 11 TCP PCBs (10 connections + 1 listen).  Production
 * lwipopts.h should set MEMP_NUM_TCP_PCB >= max_connections + 1. */
#define MEM_SIZE             (16 * 1024)
#define MEMP_NUM_NETCONN     8
#define MEMP_NUM_TCP_PCB     8
#define MEMP_NUM_UDP_PCB     8

#define LWIP_SO_RCVTIMEO     1
#define LWIP_SO_SNDTIMEO     1
#define SO_REUSE             1

#endif /* LWIPOPTS_H */
