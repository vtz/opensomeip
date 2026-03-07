/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_BYTEORDER_IMPL_H
#define SOMEIP_PLATFORM_POSIX_BYTEORDER_IMPL_H

/**
 * @brief POSIX/Host byte-order backend.
 */

#include <arpa/inet.h>

/** @implements REQ_PLATFORM_POSIX_004, REQ_PAL_BYTE_HTONS */
#define someip_htons htons
/** @implements REQ_PAL_BYTE_NTOHS */
#define someip_ntohs ntohs
/** @implements REQ_PAL_BYTE_HTONL */
#define someip_htonl htonl
/** @implements REQ_PAL_BYTE_NTOHL */
#define someip_ntohl ntohl

#endif // SOMEIP_PLATFORM_POSIX_BYTEORDER_IMPL_H
