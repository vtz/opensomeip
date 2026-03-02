/**
 * Minimal lwIP sockets stub for compile-time verification.
 */
#ifndef LWIP_SOCKETS_H
#define LWIP_SOCKETS_H

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

/* For stubs, simulate LWIP_COMPAT_SOCKETS=1 so standard names work */
#define LWIP_COMPAT_SOCKETS 1

static inline int lwip_close(int s) { return close(s); }
static inline int lwip_shutdown(int s, int how) { return shutdown(s, how); }
static inline int lwip_fcntl(int s, int cmd, int val) { return fcntl(s, cmd, val); }

#endif // LWIP_SOCKETS_H
