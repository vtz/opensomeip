/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_LWIP_BYTEORDER_IMPL_H
#define SOMEIP_PLATFORM_LWIP_BYTEORDER_IMPL_H

#include <lwip/def.h>

#define someip_htons(x) lwip_htons(x)
#define someip_ntohs(x) lwip_ntohs(x)
#define someip_htonl(x) lwip_htonl(x)
#define someip_ntohl(x) lwip_ntohl(x)

#endif // SOMEIP_PLATFORM_LWIP_BYTEORDER_IMPL_H
