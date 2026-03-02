/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_WIN32_NET_IMPL_H
#define SOMEIP_PLATFORM_WIN32_NET_IMPL_H

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#define someip_close_socket(fd) closesocket(fd)
#define someip_shutdown_socket(fd) shutdown(fd, SD_BOTH)

static inline int someip_set_nonblocking(int fd) {
    u_long mode = 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
}

static inline int someip_set_blocking(int fd) {
    u_long mode = 0;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
}

#endif // SOMEIP_PLATFORM_WIN32_NET_IMPL_H
