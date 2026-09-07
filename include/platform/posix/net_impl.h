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
#include <sys/uio.h>

using someip_socket_t = int;
constexpr someip_socket_t SOMEIP_INVALID_SOCKET = -1;

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
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    return fcntl(fd, F_SETFL,
                 static_cast<int>(static_cast<unsigned int>(flags) |
                                  static_cast<unsigned int>(O_NONBLOCK)));
}

/** @implements REQ_PAL_NET_BLOCK, REQ_PAL_NET_MODE_E01 */
static inline int someip_set_blocking(someip_socket_t fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    return fcntl(fd, F_SETFL,
                 static_cast<int>(static_cast<unsigned int>(flags) &
                                  ~static_cast<unsigned int>(O_NONBLOCK)));
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

/**
 * @brief Enable ancillary destination-address reporting on a UDP socket.
 *
 * Uses IP_PKTINFO (Linux) or IP_RECVDSTADDR (Darwin). Returns 0 on success.
 */
static inline int someip_enable_recv_dest(someip_socket_t fd) {
#if defined(__APPLE__)
    int on = 1;
    return ::setsockopt(fd, IPPROTO_IP, IP_RECVDSTADDR, &on, sizeof(on));
#elif defined(IP_PKTINFO)
    int on = 1;
    return ::setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
#else
    (void)fd;
    return -1;
#endif
}

/**
 * @brief recvfrom that also reports the IPv4 destination of the datagram.
 *
 * dest_ip is filled with a dotted IPv4 string when ancillary data is present;
 * otherwise dest_ip[0] is set to '\\0' (unknown destination).
 */
static inline ssize_t someip_recvfrom_with_dest(someip_socket_t fd, void* buf,
                                                size_t len, int flags,
                                                struct sockaddr* src,
                                                socklen_t* addrlen,
                                                char* dest_ip, size_t dest_ip_len) {
    if (dest_ip != nullptr && dest_ip_len > 0) {
        dest_ip[0] = '\0';
    }

    struct iovec iov{};
    iov.iov_base = buf;
    iov.iov_len = len;

    struct msghdr msg{};
    msg.msg_name = src;
    msg.msg_namelen = (addrlen != nullptr) ? *addrlen : 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(struct cmsghdr) char cmsg_buf[256];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    const ssize_t received = ::recvmsg(fd, &msg, flags);
    if (received < 0) {
        return received;
    }
    if (addrlen != nullptr) {
        *addrlen = msg.msg_namelen;
    }

    if (dest_ip == nullptr || dest_ip_len == 0) {
        return received;
    }

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != IPPROTO_IP) {
            continue;
        }
#if defined(__APPLE__)
        if (cmsg->cmsg_type == IP_RECVDSTADDR) {
            const auto* addr = reinterpret_cast<const struct in_addr*>(CMSG_DATA(cmsg));
            ::inet_ntop(AF_INET, addr, dest_ip, static_cast<socklen_t>(dest_ip_len));
            break;
        }
#elif defined(IP_PKTINFO)
        if (cmsg->cmsg_type == IP_PKTINFO) {
            const auto* pkt = reinterpret_cast<const struct in_pktinfo*>(CMSG_DATA(cmsg));
            ::inet_ntop(AF_INET, &pkt->ipi_addr, dest_ip, static_cast<socklen_t>(dest_ip_len));
            break;
        }
#endif
    }

    return received;
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
    struct timeval tv{};
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = static_cast<long>(timeout_ms % 1000) * 1000L;
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
