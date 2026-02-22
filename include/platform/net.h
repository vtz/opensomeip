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

#ifndef SOMEIP_PLATFORM_NET_H
#define SOMEIP_PLATFORM_NET_H

/**
 * @brief Portable socket and network header inclusion.
 *
 * All transport code should include this header instead of
 * directly including platform-specific network headers.
 */

#if defined(__ZEPHYR__)

#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/sys/byteorder.h>

/* Zephyr with CONFIG_NET_SOCKETS_POSIX_NAMES provides POSIX-compatible
 * socket API names (socket, bind, connect, send, recv, etc.).
 * Additional Zephyr-specific guards are handled inline where needed. */

#elif defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else /* POSIX (Linux, macOS) */

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#endif

/* Portable close() -- Zephyr with POSIX names already provides close();
 * Windows needs closesocket(). */
#if defined(_WIN32)
#define someip_close_socket(fd) closesocket(fd)
#else
#define someip_close_socket(fd) close(fd)
#endif

/* fcntl-based non-blocking mode is not available on Zephyr embedded targets.
 * For Zephyr, use zsock_fcntl or the socket option approach instead. */
#if defined(__ZEPHYR__) && !defined(CONFIG_NATIVE_APPLICATION)
#include <zephyr/net/socket.h>
static inline int someip_set_nonblocking(int fd) {
    /* Zephyr BSD sockets support fcntl via zsock_fcntl when
     * CONFIG_NET_SOCKETS_POSIX_NAMES is set */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
static inline int someip_set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}
#elif !defined(_WIN32)
static inline int someip_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
static inline int someip_set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}
#endif

#endif // SOMEIP_PLATFORM_NET_H
