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
 * All socket calls go through lwip_* prefixed functions so that
 * LWIP_COMPAT_SOCKETS can remain 0.  This avoids the global macros
 * (connect, read, write, ...) that lwIP defines when compat is
 * enabled, which break C++ standard library headers.
 *
 * Required lwipopts.h settings:
 *   LWIP_SOCKET          1
 *   LWIP_COMPAT_SOCKETS  0
 *   LWIP_PROVIDE_ERRNO   1   (if the C library does not provide errno)
 */

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>

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

/* ---------- Portable socket-call wrappers (lwip_ prefixed to avoid
 *            polluting the global namespace — LWIP_COMPAT_SOCKETS must
 *            be 0 so that lwIP does not define macros for connect, read,
 *            write, etc. which break C++ standard library headers) ------ */

#define someip_setsockopt  lwip_setsockopt
#define someip_getsockopt  lwip_getsockopt
#define someip_sendto      lwip_sendto
#define someip_recvfrom    lwip_recvfrom
#define someip_send        lwip_send
#define someip_recv        lwip_recv

static inline int someip_set_socket_timeout(int fd, int optname,
                                            int timeout_ms) {
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return lwip_setsockopt(fd, SOL_SOCKET, optname, &tv, sizeof(tv));
}

/* ---------- Error reporting (pass-through on lwIP) ------------------------- */

static inline int someip_socket_errno() { return errno; }

#define SOMEIP_EAGAIN      EAGAIN
#define SOMEIP_EWOULDBLOCK EWOULDBLOCK
#define SOMEIP_EINPROGRESS EINPROGRESS
#define SOMEIP_EBADF       EBADF
#define SOMEIP_EINTR       EINTR

#endif // SOMEIP_PLATFORM_LWIP_NET_IMPL_H
