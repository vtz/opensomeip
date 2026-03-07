/* POSIX-based byteorder for FreeRTOS mock testing. */
#ifndef MOCK_FREERTOS_BYTEORDER_IMPL_H
#define MOCK_FREERTOS_BYTEORDER_IMPL_H
#include <arpa/inet.h>
#define someip_htons(x) htons(x)
#define someip_ntohs(x) ntohs(x)
#define someip_htonl(x) htonl(x)
#define someip_ntohl(x) ntohl(x)
#endif
