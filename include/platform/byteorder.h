/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_BYTEORDER_H
#define SOMEIP_PLATFORM_BYTEORDER_H

/**
 * @brief Portable byte-order conversion macros.
 *
 * Wraps the platform-specific host-to-network and network-to-host
 * conversion functions so that the rest of the codebase never includes
 * <arpa/inet.h> or <winsock2.h> directly.
 */

#if defined(__ZEPHYR__)
#include <zephyr/sys/byteorder.h>
#define someip_htons(x) sys_cpu_to_be16(x)
#define someip_ntohs(x) sys_be16_to_cpu(x)
#define someip_htonl(x) sys_cpu_to_be32(x)
#define someip_ntohl(x) sys_be32_to_cpu(x)

#elif defined(_WIN32)
#include <winsock2.h>
#define someip_htons htons
#define someip_ntohs ntohs
#define someip_htonl htonl
#define someip_ntohl ntohl

#else
#include <arpa/inet.h>
#define someip_htons htons
#define someip_ntohs ntohs
#define someip_htonl htonl
#define someip_ntohl ntohl

#endif

#endif // SOMEIP_PLATFORM_BYTEORDER_H
