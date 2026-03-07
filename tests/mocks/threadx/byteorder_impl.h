/* POSIX-based byteorder for ThreadX mock testing. */
#ifndef MOCK_THREADX_BYTEORDER_IMPL_H
#define MOCK_THREADX_BYTEORDER_IMPL_H
#include <arpa/inet.h>
#define someip_htons(x) htons(x)
#define someip_ntohs(x) ntohs(x)
#define someip_htonl(x) htonl(x)
#define someip_ntohl(x) ntohl(x)
#endif
