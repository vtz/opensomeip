/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_ZEPHYR_NET_IMPL_H
#define SOMEIP_PLATFORM_ZEPHYR_NET_IMPL_H

#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/sys/byteorder.h>

/*
 * Map standard POSIX socket names to Zephyr's zsock_ API.
 * CONFIG_NET_SOCKETS_POSIX_NAMES was removed in Zephyr 4.3.x;
 * these macros replicate its behaviour for our transport layer.
 *
 * NOTE: "connect" is intentionally omitted -- it clashes with the
 * ITransport::connect() virtual method.  Transport code that needs
 * the BSD connect() syscall should call zsock_connect() directly.
 */
#define socket(...)       zsock_socket(__VA_ARGS__)
#define close(...)        zsock_close(__VA_ARGS__)
#define bind(...)         zsock_bind(__VA_ARGS__)
#define listen(...)       zsock_listen(__VA_ARGS__)
#define accept(...)       zsock_accept(__VA_ARGS__)
#define send(...)         zsock_send(__VA_ARGS__)
#define sendto(...)       zsock_sendto(__VA_ARGS__)
#define recv(...)         zsock_recv(__VA_ARGS__)
#define recvfrom(...)     zsock_recvfrom(__VA_ARGS__)
#define setsockopt(...)   zsock_setsockopt(__VA_ARGS__)
#define getsockopt(...)   zsock_getsockopt(__VA_ARGS__)
#define getsockname(...)  zsock_getsockname(__VA_ARGS__)
#define fcntl(...)        zsock_fcntl(__VA_ARGS__)
#define select(...)       zsock_select(__VA_ARGS__)
#define poll(...)         zsock_poll(__VA_ARGS__)
#define inet_ntop(...)    zsock_inet_ntop(__VA_ARGS__)
#define inet_pton(...)    zsock_inet_pton(__VA_ARGS__)

#define fd_set            zsock_fd_set
#define timeval           zsock_timeval
#undef  FD_ZERO
#define FD_ZERO(...)      ZSOCK_FD_ZERO(__VA_ARGS__)
#undef  FD_SET
#define FD_SET(...)       ZSOCK_FD_SET(__VA_ARGS__)
#undef  FD_CLR
#define FD_CLR(...)       ZSOCK_FD_CLR(__VA_ARGS__)
#undef  FD_ISSET
#define FD_ISSET(...)     ZSOCK_FD_ISSET(__VA_ARGS__)

#define F_GETFL           ZVFS_F_GETFL
#define F_SETFL           ZVFS_F_SETFL
#define O_NONBLOCK        ZVFS_O_NONBLOCK

#define SHUT_RD           ZSOCK_SHUT_RD
#define SHUT_WR           ZSOCK_SHUT_WR
#define SHUT_RDWR         ZSOCK_SHUT_RDWR

#ifndef INADDR_NONE
#define INADDR_NONE       ((uint32_t)0xffffffff)
#endif

typedef uint32_t in_addr_t;

static inline uint32_t someip_zephyr_inet_addr(const char *cp) {
    struct in_addr addr;
    if (zsock_inet_pton(AF_INET, cp, &addr) == 1) {
        return addr.s_addr;
    }
    return INADDR_NONE;
}
#define inet_addr(cp) someip_zephyr_inet_addr(cp)

#define someip_close_socket(fd) zsock_close(fd)
#define someip_shutdown_socket(fd) zsock_shutdown(fd, ZSOCK_SHUT_RDWR)

static inline int someip_set_nonblocking(int fd) {
    int flags = zsock_fcntl(fd, ZVFS_F_GETFL, 0);
    if (flags < 0) return -1;
    return zsock_fcntl(fd, ZVFS_F_SETFL, flags | ZVFS_O_NONBLOCK);
}

static inline int someip_set_blocking(int fd) {
    int flags = zsock_fcntl(fd, ZVFS_F_GETFL, 0);
    if (flags < 0) return -1;
    return zsock_fcntl(fd, ZVFS_F_SETFL, flags & ~ZVFS_O_NONBLOCK);
}

#endif // SOMEIP_PLATFORM_ZEPHYR_NET_IMPL_H
