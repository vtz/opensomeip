/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_LWIP_NET_IMPL_H
#define SOMEIP_PLATFORM_LWIP_NET_IMPL_H

/**
 * @brief lwIP socket-API networking backend.
 *
 * lwIP's socket layer (lwip/sockets.h) is intentionally POSIX-compatible.
 * When LWIP_COMPAT_SOCKETS is enabled in lwipopts.h, standard names
 * (socket, bind, send, ...) are already available and no macro mapping
 * is needed.  When it is disabled, we provide explicit macros below.
 *
 * Required lwipopts.h settings:
 *   LWIP_SOCKET          1
 *   LWIP_COMPAT_SOCKETS  1   (recommended; if 0, macros below activate)
 *   LWIP_PROVIDE_ERRNO   1   (if the C library does not provide errno)
 */

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>

#if !LWIP_COMPAT_SOCKETS
#define socket(...)       lwip_socket(__VA_ARGS__)
#define close(...)        lwip_close(__VA_ARGS__)
#define bind(...)         lwip_bind(__VA_ARGS__)
#define listen(...)       lwip_listen(__VA_ARGS__)
#define accept(...)       lwip_accept(__VA_ARGS__)
#define connect(...)      lwip_connect(__VA_ARGS__)
#define send(...)         lwip_send(__VA_ARGS__)
#define sendto(...)       lwip_sendto(__VA_ARGS__)
#define recv(...)         lwip_recv(__VA_ARGS__)
#define recvfrom(...)     lwip_recvfrom(__VA_ARGS__)
#define setsockopt(...)   lwip_setsockopt(__VA_ARGS__)
#define getsockopt(...)   lwip_getsockopt(__VA_ARGS__)
#define getsockname(...)  lwip_getsockname(__VA_ARGS__)
#define shutdown(...)     lwip_shutdown(__VA_ARGS__)
#define fcntl(...)        lwip_fcntl(__VA_ARGS__)
#define select(...)       lwip_select(__VA_ARGS__)
#define poll(...)         lwip_poll(__VA_ARGS__)
#define inet_ntop(...)    lwip_inet_ntop(__VA_ARGS__)
#define inet_pton(...)    lwip_inet_pton(__VA_ARGS__)
#endif /* !LWIP_COMPAT_SOCKETS */

using someip_socket_t = int;
#define SOMEIP_INVALID_SOCKET (-1)

/** @implements REQ_PLATFORM_LWIP_001 */
#define someip_close_socket(fd) lwip_close(fd)
/** @implements REQ_PLATFORM_LWIP_001 */
#define someip_shutdown_socket(fd) lwip_shutdown(fd, SHUT_RDWR)

/** @implements REQ_PLATFORM_LWIP_001 */
static inline int someip_set_nonblocking(int fd) {
    int flags = lwip_fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/** @implements REQ_PLATFORM_LWIP_001 */
static inline int someip_set_blocking(int fd) {
    int flags = lwip_fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return lwip_fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

/* ---------- Portable socket-call wrappers (pass-through on lwIP) ----------- */

#define someip_setsockopt  setsockopt
#define someip_getsockopt  getsockopt
#define someip_sendto      sendto
#define someip_recvfrom    recvfrom
#define someip_send        send
#define someip_recv        recv

static inline int someip_set_socket_timeout(int fd, int optname,
                                            int timeout_ms) {
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, optname, &tv, sizeof(tv));
}

/* ---------- Error reporting (pass-through on lwIP) ------------------------- */

static inline int someip_socket_errno() { return errno; }

#define SOMEIP_EAGAIN      EAGAIN
#define SOMEIP_EWOULDBLOCK EWOULDBLOCK
#define SOMEIP_EINPROGRESS EINPROGRESS
#define SOMEIP_EBADF       EBADF
#define SOMEIP_EINTR       EINTR

#endif // SOMEIP_PLATFORM_LWIP_NET_IMPL_H
