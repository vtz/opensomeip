/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_WIN32_NET_IMPL_H
#define SOMEIP_PLATFORM_WIN32_NET_IMPL_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <BaseTsd.h>
#pragma comment(lib, "ws2_32.lib")

using ssize_t = SSIZE_T;
using in_addr_t = u_long;
using someip_socket_t = SOCKET;
#define SOMEIP_INVALID_SOCKET INVALID_SOCKET

#define someip_close_socket(fd) closesocket(fd)
#define someip_shutdown_socket(fd) shutdown(fd, SD_BOTH)

static inline int someip_set_nonblocking(someip_socket_t fd) {
    u_long mode = 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
}

static inline int someip_set_blocking(someip_socket_t fd) {
    u_long mode = 0;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
}

/* ---------- Portable socket-call wrappers (Winsock uses char* not void*) --- */

/** @implements REQ_PAL_NET_SOCKOPT */
static inline int someip_setsockopt(someip_socket_t fd, int level, int optname,
                                    const void* optval, int optlen) {
    return setsockopt(fd, level, optname,
                      reinterpret_cast<const char*>(optval), optlen);
}

/** @implements REQ_PAL_NET_SOCKOPT */
static inline int someip_getsockopt(someip_socket_t fd, int level, int optname,
                                    void* optval, socklen_t* optlen) {
    return getsockopt(fd, level, optname,
                      reinterpret_cast<char*>(optval),
                      reinterpret_cast<int*>(optlen));
}

/** @implements REQ_PAL_NET_SEND */
static inline ssize_t someip_sendto(someip_socket_t fd, const void* buf, size_t len,
                                    int flags, const struct sockaddr* dest,
                                    socklen_t addrlen) {
    return sendto(fd, reinterpret_cast<const char*>(buf),
                  static_cast<int>(len), flags, dest, addrlen);
}

/** @implements REQ_PAL_NET_RECV */
static inline ssize_t someip_recvfrom(someip_socket_t fd, void* buf, size_t len,
                                      int flags, struct sockaddr* src,
                                      socklen_t* addrlen) {
    return recvfrom(fd, reinterpret_cast<char*>(buf),
                    static_cast<int>(len), flags, src, addrlen);
}

/** @implements REQ_PAL_NET_SEND */
static inline ssize_t someip_send(someip_socket_t fd, const void* buf, size_t len,
                                  int flags) {
    return send(fd, reinterpret_cast<const char*>(buf),
                static_cast<int>(len), flags);
}

/** @implements REQ_PAL_NET_RECV */
static inline ssize_t someip_recv(someip_socket_t fd, void* buf, size_t len, int flags) {
    return recv(fd, reinterpret_cast<char*>(buf),
                static_cast<int>(len), flags);
}

/* ---------- Socket timeout helper (Windows uses DWORD ms, not timeval) ----- */

static inline int someip_set_socket_timeout(someip_socket_t fd, int optname,
                                            int timeout_ms) {
    DWORD ms = static_cast<DWORD>(timeout_ms);
    return setsockopt(fd, SOL_SOCKET, optname,
                      reinterpret_cast<const char*>(&ms), sizeof(ms));
}

/* ---------- Error reporting (Winsock uses WSAGetLastError, not errno) ------- */

static inline int someip_socket_errno() { return WSAGetLastError(); }

#define SOMEIP_EAGAIN      WSAEWOULDBLOCK
#define SOMEIP_EWOULDBLOCK WSAEWOULDBLOCK
#define SOMEIP_EINPROGRESS WSAEWOULDBLOCK  /* non-blocking connect() */
#define SOMEIP_EBADF       WSAENOTSOCK
#define SOMEIP_EINTR       WSAEINTR

#endif // SOMEIP_PLATFORM_WIN32_NET_IMPL_H
