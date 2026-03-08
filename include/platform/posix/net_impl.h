/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_NET_IMPL_H
#define SOMEIP_PLATFORM_POSIX_NET_IMPL_H

/**
 * @brief POSIX/Host networking backend.
 */

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

/** @implements REQ_PLATFORM_POSIX_003, REQ_PAL_NET_CLOSE */
#define someip_close_socket(fd) close(fd)
/** @implements REQ_PAL_NET_SHUTDOWN */
#define someip_shutdown_socket(fd) shutdown(fd, SHUT_RDWR)

/** @implements REQ_PAL_NET_NONBLOCK, REQ_PAL_NET_MODE_E01 */
static inline int someip_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/** @implements REQ_PAL_NET_BLOCK, REQ_PAL_NET_MODE_E01 */
static inline int someip_set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

#endif // SOMEIP_PLATFORM_POSIX_NET_IMPL_H
