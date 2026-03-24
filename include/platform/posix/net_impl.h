/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_POSIX_NET_IMPL_H
#define SOMEIP_PLATFORM_POSIX_NET_IMPL_H

/**
 * @brief POSIX/Host networking backend.
 *
 * Every socket operation is wrapped in a someip_* inline function so that
 * transport-layer code never calls raw POSIX/lwIP/Winsock symbols directly.
 * On POSIX the wrappers are trivial pass-throughs; the compiler inlines them
 * at zero cost.
 */

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

using someip_socket_t = int;
#define SOMEIP_INVALID_SOCKET (-1)

/* ---------- Socket lifecycle ----------------------------------------------- */

/** @implements REQ_PLATFORM_POSIX_003, REQ_PAL_NET_CLOSE */
static inline int someip_close_socket(someip_socket_t fd) {
    return close(fd);
}

/** @implements REQ_PAL_NET_SHUTDOWN */
static inline int someip_shutdown_socket(someip_socket_t fd) {
    return shutdown(fd, SHUT_RDWR);
}

/** @implements REQ_PAL_NET_NONBLOCK, REQ_PAL_NET_MODE_E01 */
static inline int someip_set_nonblocking(someip_socket_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/** @implements REQ_PAL_NET_BLOCK, REQ_PAL_NET_MODE_E01 */
static inline int someip_set_blocking(someip_socket_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

/* ---------- Socket creation & connection ----------------------------------- */

static inline someip_socket_t someip_socket(int domain, int type, int protocol) {
    return ::socket(domain, type, protocol);
}

static inline int someip_bind(someip_socket_t fd, const struct sockaddr* addr,
                              socklen_t addrlen) {
    return ::bind(fd, addr, addrlen);
}

static inline int someip_listen(someip_socket_t fd, int backlog) {
    return ::listen(fd, backlog);
}

static inline int someip_connect(someip_socket_t fd, const struct sockaddr* addr,
                                 socklen_t addrlen) {
    return ::connect(fd, addr, addrlen);
}

static inline someip_socket_t someip_accept(someip_socket_t fd,
                                            struct sockaddr* addr,
                                            socklen_t* addrlen) {
    return ::accept(fd, addr, addrlen);
}

static inline int someip_getsockname(someip_socket_t fd, struct sockaddr* addr,
                                     socklen_t* addrlen) {
    return ::getsockname(fd, addr, addrlen);
}

/* ---------- I/O multiplexing ----------------------------------------------- */

static inline int someip_select(int nfds, fd_set* readfds, fd_set* writefds,
                                fd_set* exceptfds, struct timeval* timeout) {
    return ::select(nfds, readfds, writefds, exceptfds, timeout);
}

/* ---------- Address conversion --------------------------------------------- */

static inline in_addr_t someip_inet_addr(const char* cp) {
    return ::inet_addr(cp);
}

static inline int someip_inet_pton(int af, const char* src, void* dst) {
    return ::inet_pton(af, src, dst);
}

static inline const char* someip_inet_ntop(int af, const void* src,
                                           char* dst, socklen_t size) {
    return ::inet_ntop(af, src, dst, size);
}

/* ---------- Socket options ------------------------------------------------- */

static inline int someip_setsockopt(someip_socket_t fd, int level, int optname,
                                    const void* optval, socklen_t optlen) {
    return ::setsockopt(fd, level, optname, optval, optlen);
}

static inline int someip_getsockopt(someip_socket_t fd, int level, int optname,
                                    void* optval, socklen_t* optlen) {
    return ::getsockopt(fd, level, optname, optval, optlen);
}

/* ---------- Data transfer -------------------------------------------------- */

static inline ssize_t someip_sendto(someip_socket_t fd, const void* buf,
                                    size_t len, int flags,
                                    const struct sockaddr* dest,
                                    socklen_t addrlen) {
    return ::sendto(fd, buf, len, flags, dest, addrlen);
}

static inline ssize_t someip_recvfrom(someip_socket_t fd, void* buf,
                                      size_t len, int flags,
                                      struct sockaddr* src,
                                      socklen_t* addrlen) {
    return ::recvfrom(fd, buf, len, flags, src, addrlen);
}

static inline ssize_t someip_send(someip_socket_t fd, const void* buf,
                                  size_t len, int flags) {
    return ::send(fd, buf, len, flags);
}

static inline ssize_t someip_recv(someip_socket_t fd, void* buf,
                                  size_t len, int flags) {
    return ::recv(fd, buf, len, flags);
}

/* ---------- Timeout helper ------------------------------------------------- */

static inline int someip_set_socket_timeout(someip_socket_t fd, int optname,
                                            int timeout_ms) {
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return ::setsockopt(fd, SOL_SOCKET, optname, &tv, sizeof(tv));
}

/* ---------- Error reporting ------------------------------------------------ */

static inline int someip_socket_errno() { return errno; }

#define SOMEIP_EAGAIN      EAGAIN
#define SOMEIP_EWOULDBLOCK EWOULDBLOCK
#define SOMEIP_EINPROGRESS EINPROGRESS
#define SOMEIP_EBADF       EBADF
#define SOMEIP_EINTR       EINTR

#endif // SOMEIP_PLATFORM_POSIX_NET_IMPL_H
