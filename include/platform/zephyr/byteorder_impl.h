/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_ZEPHYR_BYTEORDER_IMPL_H
#define SOMEIP_PLATFORM_ZEPHYR_BYTEORDER_IMPL_H

#include <zephyr/sys/byteorder.h>

#define someip_htons(x) sys_cpu_to_be16(x)
#define someip_ntohs(x) sys_be16_to_cpu(x)
#define someip_htonl(x) sys_cpu_to_be32(x)
#define someip_ntohl(x) sys_be32_to_cpu(x)

#endif // SOMEIP_PLATFORM_ZEPHYR_BYTEORDER_IMPL_H
