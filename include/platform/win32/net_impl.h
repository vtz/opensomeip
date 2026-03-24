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

/* ---------- Socket lifecycle ----------------------------------------------- */

static inline int someip_close_socket(someip_socket_t fd) {
    return closesocket(fd);
}

static inline int someip_shutdown_socket(someip_socket_t fd) {
    return shutdown(fd, SD_BOTH);
}

static inline int someip_set_nonblocking(someip_socket_t fd) {
    u_long mode = 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
}

static inline int someip_set_blocking(someip_socket_t fd) {
    u_long mode = 0;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
}

/* ---------- Socket creation & connection ----------------------------------- */

static inline someip_socket_t someip_socket(int domain, int type, int protocol) {
    return ::socket(domain, type, protocol);
}

static inline int someip_bind(someip_socket_t fd, const struct sockaddr* addr,
                              socklen_t addrlen) {
    return ::bind(fd, addr, static_cast<int>(addrlen));
}

static inline int someip_listen(someip_socket_t fd, int backlog) {
    return ::listen(fd, backlog);
}

static inline int someip_connect(someip_socket_t fd, const struct sockaddr* addr,
                                 socklen_t addrlen) {
    return ::connect(fd, addr, static_cast<int>(addrlen));
}

static inline someip_socket_t someip_accept(someip_socket_t fd,
                                            struct sockaddr* addr,
                                            socklen_t* addrlen) {
    return ::accept(fd, addr, reinterpret_cast<int*>(addrlen));
}

static inline int someip_getsockname(someip_socket_t fd, struct sockaddr* addr,
                                     socklen_t* addrlen) {
    return ::getsockname(fd, addr, reinterpret_cast<int*>(addrlen));
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
    return ::inet_ntop(af, src, dst, static_cast<size_t>(size));
}

/* ---------- Socket options (Winsock uses char*, not void*) ------------------ */

static inline int someip_setsockopt(someip_socket_t fd, int level, int optname,
                                    const void* optval, socklen_t optlen) {
    return setsockopt(fd, level, optname,
                      reinterpret_cast<const char*>(optval),
                      static_cast<int>(optlen));
}

static inline int someip_getsockopt(someip_socket_t fd, int level, int optname,
                                    void* optval, socklen_t* optlen) {
    return getsockopt(fd, level, optname,
                      reinterpret_cast<char*>(optval),
                      reinterpret_cast<int*>(optlen));
}

/* ---------- Data transfer (Winsock uses char*, int len) --------------------- */

static inline ssize_t someip_sendto(someip_socket_t fd, const void* buf, size_t len,
                                    int flags, const struct sockaddr* dest,
                                    socklen_t addrlen) {
    return sendto(fd, reinterpret_cast<const char*>(buf),
                  static_cast<int>(len), flags, dest, static_cast<int>(addrlen));
}

static inline ssize_t someip_recvfrom(someip_socket_t fd, void* buf, size_t len,
                                      int flags, struct sockaddr* src,
                                      socklen_t* addrlen) {
    return recvfrom(fd, reinterpret_cast<char*>(buf),
                    static_cast<int>(len), flags, src,
                    reinterpret_cast<int*>(addrlen));
}

static inline ssize_t someip_send(someip_socket_t fd, const void* buf, size_t len,
                                  int flags) {
    return send(fd, reinterpret_cast<const char*>(buf),
                static_cast<int>(len), flags);
}

static inline ssize_t someip_recv(someip_socket_t fd, void* buf, size_t len, int flags) {
    return recv(fd, reinterpret_cast<char*>(buf),
                static_cast<int>(len), flags);
}

/* ---------- Timeout helper (Windows uses DWORD ms, not timeval) ------------ */

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
