/* POSIX-based net for ThreadX mock testing. */
#ifndef MOCK_THREADX_NET_IMPL_H
#define MOCK_THREADX_NET_IMPL_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
inline int someip_close_socket(int fd) { return close(fd); }
inline void someip_shutdown_socket(int fd) { shutdown(fd, SHUT_RDWR); }
inline int someip_set_nonblocking(int fd) {
    int f = fcntl(fd, F_GETFL, 0);
    return (f < 0) ? -1 : (fcntl(fd, F_SETFL, f | O_NONBLOCK) < 0 ? -1 : 0);
}
inline int someip_set_blocking(int fd) {
    int f = fcntl(fd, F_GETFL, 0);
    return (f < 0) ? -1 : (fcntl(fd, F_SETFL, f & ~O_NONBLOCK) < 0 ? -1 : 0);
}
#endif
