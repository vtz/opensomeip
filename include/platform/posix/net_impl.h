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
#include <errno.h>

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

/* ---------- Portable socket-call wrappers (pass-through on POSIX) ---------- */

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

/* ---------- Error reporting (pass-through on POSIX) ------------------------ */

static inline int someip_socket_errno() { return errno; }

#define SOMEIP_EAGAIN      EAGAIN
#define SOMEIP_EWOULDBLOCK EWOULDBLOCK
#define SOMEIP_EINPROGRESS EINPROGRESS
#define SOMEIP_EBADF       EBADF
#define SOMEIP_EINTR       EINTR

#endif // SOMEIP_PLATFORM_POSIX_NET_IMPL_H
