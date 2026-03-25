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
 * Zephyr 4.3.x removed CONFIG_NET_SOCKETS_POSIX_NAMES.
 * All opensomeip transport code uses someip_* inline wrappers defined
 * below, so we do NOT define function-like macros (connect, send, …)
 * — those would conflict with C++ method names.
 *
 * We DO provide type and constant aliases that the portable transport
 * code relies on (fd_set, FD_ZERO, timeval, …).
 */
#ifndef FD_ZERO
#define fd_set            zsock_fd_set
#define FD_ZERO(...)      ZSOCK_FD_ZERO(__VA_ARGS__)
#define FD_SET(...)       ZSOCK_FD_SET(__VA_ARGS__)
#define FD_CLR(...)       ZSOCK_FD_CLR(__VA_ARGS__)
#define FD_ISSET(...)     ZSOCK_FD_ISSET(__VA_ARGS__)
#endif

#ifndef F_GETFL
#define F_GETFL           ZVFS_F_GETFL
#define F_SETFL           ZVFS_F_SETFL
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK        ZVFS_O_NONBLOCK
#endif

#ifndef SHUT_RDWR
#define SHUT_RD           ZSOCK_SHUT_RD
#define SHUT_WR           ZSOCK_SHUT_WR
#define SHUT_RDWR         ZSOCK_SHUT_RDWR
#endif

#ifndef INADDR_NONE
#define INADDR_NONE       ((uint32_t)0xffffffff)
#endif

typedef uint32_t in_addr_t;
using someip_socket_t = int;
#define SOMEIP_INVALID_SOCKET (-1)

/* ---------- Zephyr inet_addr helper (Zephyr lacks inet_addr) --------------- */

static inline uint32_t someip_zephyr_inet_addr(const char *cp) {
    struct in_addr addr;
    if (zsock_inet_pton(AF_INET, cp, &addr) == 1) {
        return addr.s_addr;
    }
    return INADDR_NONE;
}
#define inet_addr(cp) someip_zephyr_inet_addr(cp)

/* ---------- Socket lifecycle ----------------------------------------------- */

static inline int someip_close_socket(someip_socket_t fd) {
    return zsock_close(fd);
}

static inline int someip_shutdown_socket(someip_socket_t fd) {
    return zsock_shutdown(fd, ZSOCK_SHUT_RDWR);
}

static inline int someip_set_nonblocking(someip_socket_t fd) {
    int flags = zsock_fcntl(fd, ZVFS_F_GETFL, 0);
    if (flags < 0) return -1;
    return zsock_fcntl(fd, ZVFS_F_SETFL, flags | ZVFS_O_NONBLOCK);
}

static inline int someip_set_blocking(someip_socket_t fd) {
    int flags = zsock_fcntl(fd, ZVFS_F_GETFL, 0);
    if (flags < 0) return -1;
    return zsock_fcntl(fd, ZVFS_F_SETFL, flags & ~ZVFS_O_NONBLOCK);
}

/* ---------- Socket creation & connection ----------------------------------- */

static inline someip_socket_t someip_socket(int domain, int type, int protocol) {
    return zsock_socket(domain, type, protocol);
}

static inline int someip_bind(someip_socket_t fd, const struct sockaddr* addr,
                              socklen_t addrlen) {
    return zsock_bind(fd, addr, addrlen);
}

static inline int someip_listen(someip_socket_t fd, int backlog) {
    return zsock_listen(fd, backlog);
}

static inline int someip_connect(someip_socket_t fd, const struct sockaddr* addr,
                                 socklen_t addrlen) {
    return zsock_connect(fd, addr, addrlen);
}

static inline someip_socket_t someip_accept(someip_socket_t fd,
                                            struct sockaddr* addr,
                                            socklen_t* addrlen) {
    return zsock_accept(fd, addr, addrlen);
}

static inline int someip_getsockname(someip_socket_t fd, struct sockaddr* addr,
                                     socklen_t* addrlen) {
    return zsock_getsockname(fd, addr, addrlen);
}

/* ---------- I/O multiplexing ----------------------------------------------- */

static inline int someip_select(int nfds, fd_set* readfds,
                                fd_set* writefds,
                                fd_set* exceptfds,
                                struct timeval* timeout) {
    return zsock_select(nfds, readfds, writefds, exceptfds, timeout);
}

/* ---------- Address conversion --------------------------------------------- */

static inline in_addr_t someip_inet_addr(const char* cp) {
    return someip_zephyr_inet_addr(cp);
}

static inline int someip_inet_pton(int af, const char* src, void* dst) {
    return zsock_inet_pton(af, src, dst);
}

static inline const char* someip_inet_ntop(int af, const void* src,
                                           char* dst, socklen_t size) {
    return zsock_inet_ntop(af, src, dst, size);
}

/* ---------- Socket options ------------------------------------------------- */

static inline int someip_setsockopt(someip_socket_t fd, int level, int optname,
                                    const void* optval, socklen_t optlen) {
    return zsock_setsockopt(fd, level, optname, optval, optlen);
}

static inline int someip_getsockopt(someip_socket_t fd, int level, int optname,
                                    void* optval, socklen_t* optlen) {
    return zsock_getsockopt(fd, level, optname, optval, optlen);
}

/* ---------- Data transfer -------------------------------------------------- */

static inline ssize_t someip_sendto(someip_socket_t fd, const void* buf,
                                    size_t len, int flags,
                                    const struct sockaddr* dest,
                                    socklen_t addrlen) {
    return zsock_sendto(fd, buf, len, flags, dest, addrlen);
}

static inline ssize_t someip_recvfrom(someip_socket_t fd, void* buf,
                                      size_t len, int flags,
                                      struct sockaddr* src,
                                      socklen_t* addrlen) {
    return zsock_recvfrom(fd, buf, len, flags, src, addrlen);
}

static inline ssize_t someip_send(someip_socket_t fd, const void* buf,
                                  size_t len, int flags) {
    return zsock_send(fd, buf, len, flags);
}

static inline ssize_t someip_recv(someip_socket_t fd, void* buf,
                                  size_t len, int flags) {
    return zsock_recv(fd, buf, len, flags);
}

/* ---------- Timeout helper ------------------------------------------------- */

static inline int someip_set_socket_timeout(someip_socket_t fd, int optname,
                                            int timeout_ms) {
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return zsock_setsockopt(fd, SOL_SOCKET, optname, &tv, sizeof(tv));
}

/* ---------- Error reporting ------------------------------------------------ */

static inline int someip_socket_errno() { return errno; }

#define SOMEIP_EAGAIN      EAGAIN
#define SOMEIP_EWOULDBLOCK EWOULDBLOCK
#define SOMEIP_EINPROGRESS EINPROGRESS
#define SOMEIP_EBADF       EBADF
#define SOMEIP_EINTR       EINTR

#endif // SOMEIP_PLATFORM_ZEPHYR_NET_IMPL_H
