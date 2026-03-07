/*
 * POSIX-based byteorder_impl.h for Zephyr mock testing.
 * Shadows include/platform/zephyr/byteorder_impl.h so the test
 * compiles without real Zephyr headers.
 */

#ifndef MOCK_ZEPHYR_BYTEORDER_IMPL_H
#define MOCK_ZEPHYR_BYTEORDER_IMPL_H

#include <arpa/inet.h>

#define someip_htons(x) htons(x)
#define someip_ntohs(x) ntohs(x)
#define someip_htonl(x) htonl(x)
#define someip_ntohl(x) ntohl(x)

#endif
