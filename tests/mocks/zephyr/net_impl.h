/*
 * POSIX-based net_impl.h for Zephyr mock testing.
 * Shadows include/platform/zephyr/net_impl.h so the test
 * compiles without real Zephyr headers.
 */

#ifndef MOCK_ZEPHYR_NET_IMPL_H
#define MOCK_ZEPHYR_NET_IMPL_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

using someip_socket_t = int;
#define SOMEIP_INVALID_SOCKET (-1)

inline int someip_close_socket(int fd) { return close(fd); }

inline int someip_shutdown_socket(int fd) { return shutdown(fd, SHUT_RDWR); }

inline int someip_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ? -1 : 0;
}

inline int someip_set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0 ? -1 : 0;
}

#endif
