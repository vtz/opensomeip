/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_BAREMETAL_BYTEORDER_IMPL_H
#define SOMEIP_PLATFORM_BAREMETAL_BYTEORDER_IMPL_H

/**
 * @brief Bare-metal byte-order backend using GCC/Clang builtins.
 *
 * ARM Cortex-M is little-endian; network byte order is big-endian,
 * so htons/htonl are byte-swaps.
 */

#define someip_htons(x) __builtin_bswap16(x)
#define someip_ntohs(x) __builtin_bswap16(x)
#define someip_htonl(x) __builtin_bswap32(x)
#define someip_ntohl(x) __builtin_bswap32(x)

#endif // SOMEIP_PLATFORM_BAREMETAL_BYTEORDER_IMPL_H
