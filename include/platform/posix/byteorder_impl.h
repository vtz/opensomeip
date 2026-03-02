/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_BYTEORDER_IMPL_H
#define SOMEIP_PLATFORM_POSIX_BYTEORDER_IMPL_H

#include <arpa/inet.h>

#define someip_htons htons
#define someip_ntohs ntohs
#define someip_htonl htonl
#define someip_ntohl ntohl

#endif // SOMEIP_PLATFORM_POSIX_BYTEORDER_IMPL_H
