/**
 * Minimal lwIP def.h stub for compile-time verification.
 */
#ifndef LWIP_DEF_H
#define LWIP_DEF_H

#include <stdint.h>

static inline uint16_t lwip_htons(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}

static inline uint16_t lwip_ntohs(uint16_t x) {
    return lwip_htons(x);
}

static inline uint32_t lwip_htonl(uint32_t x) {
    return ((x >> 24) & 0x000000FFU) |
           ((x >>  8) & 0x0000FF00U) |
           ((x <<  8) & 0x00FF0000U) |
           ((x << 24) & 0xFF000000U);
}

static inline uint32_t lwip_ntohl(uint32_t x) {
    return lwip_htonl(x);
}

#endif // LWIP_DEF_H
