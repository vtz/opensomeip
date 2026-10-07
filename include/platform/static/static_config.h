/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_STATIC_CONFIG_H
#define SOMEIP_PLATFORM_STATIC_CONFIG_H

/**
 * @brief Compile-time capacity limits for the static-allocation PAL backend.
 *
 * All values are overridable via -D on the compiler command line.
 *
 * @implements REQ_PAL_STATIC_CONFIG
 */

#include "e2e/e2e_limits.h"

#ifndef SOMEIP_MAX_PAYLOAD_SIZE
#define SOMEIP_MAX_PAYLOAD_SIZE 1400
#endif

/* 16-byte SOME/IP header + unprotected E2E prefix + E2E header + payload.
 * A message this large may exceed the medium byte-pool tier and use a large
 * slab. Overriding SOMEIP_MAX_PAYLOAD_SIZE without also setting
 * SOMEIP_MAX_MESSAGE_SIZE grows this default with the payload. */
#ifndef SOMEIP_MAX_MESSAGE_SIZE
#define SOMEIP_MAX_MESSAGE_SIZE \
    (16 + SOMEIP_MAX_E2E_PREFIX_SIZE + SOMEIP_MAX_E2E_HEADER_SIZE + SOMEIP_MAX_PAYLOAD_SIZE)
#endif

#ifndef SOMEIP_MAX_TCP_PAYLOAD_SIZE
#define SOMEIP_MAX_TCP_PAYLOAD_SIZE 65535
#endif

#ifndef SOMEIP_MESSAGE_POOL_SIZE
#define SOMEIP_MESSAGE_POOL_SIZE 16
#endif

#ifndef SOMEIP_MAX_SESSIONS
#define SOMEIP_MAX_SESSIONS 64
#endif

#ifndef SOMEIP_MAX_SD_ENTRIES
#define SOMEIP_MAX_SD_ENTRIES 32
#endif

#ifndef SOMEIP_MAX_MULTICAST_GROUPS
#define SOMEIP_MAX_MULTICAST_GROUPS 8
#endif

#ifndef SOMEIP_MAX_CONCURRENT_TP
#define SOMEIP_MAX_CONCURRENT_TP 10
#endif

#ifndef SOMEIP_MAX_TP_REASSEMBLY_BUFFERS
#define SOMEIP_MAX_TP_REASSEMBLY_BUFFERS SOMEIP_MAX_CONCURRENT_TP
#endif

#ifndef SOMEIP_MAX_RECEIVE_QUEUE
#define SOMEIP_MAX_RECEIVE_QUEUE 32
#endif

#ifndef SOMEIP_BYTE_POOL_SMALL_COUNT
#define SOMEIP_BYTE_POOL_SMALL_COUNT 32
#endif

#ifndef SOMEIP_BYTE_POOL_MEDIUM_COUNT
#define SOMEIP_BYTE_POOL_MEDIUM_COUNT 16
#endif

#ifndef SOMEIP_BYTE_POOL_LARGE_COUNT
#define SOMEIP_BYTE_POOL_LARGE_COUNT 4
#endif

#ifndef SOMEIP_BYTE_POOL_SMALL_SIZE
#define SOMEIP_BYTE_POOL_SMALL_SIZE 256
#endif

/* Default max message is 16 + 64 + 64 + 1400 = 1544, so the medium tier
 * must be at least that large or a full E2E message falls through to the
 * large slab. 1600 keeps the historical ~1500-byte tier in the same band. */
#ifndef SOMEIP_BYTE_POOL_MEDIUM_SIZE
#define SOMEIP_BYTE_POOL_MEDIUM_SIZE 1600
#endif

#ifndef SOMEIP_BYTE_POOL_LARGE_SIZE
#define SOMEIP_BYTE_POOL_LARGE_SIZE 65536
#endif

#ifndef SOMEIP_DEFAULT_VECTOR_CAPACITY
#define SOMEIP_DEFAULT_VECTOR_CAPACITY 32
#endif

#ifndef SOMEIP_DEFAULT_STRING_CAPACITY
#define SOMEIP_DEFAULT_STRING_CAPACITY 64
#endif

#ifndef SOMEIP_DEFAULT_MAP_CAPACITY
#define SOMEIP_DEFAULT_MAP_CAPACITY 32
#endif

#ifndef SOMEIP_DEFAULT_QUEUE_CAPACITY
#define SOMEIP_DEFAULT_QUEUE_CAPACITY 32
#endif

#ifndef SOMEIP_DEFAULT_CALLBACK_CAPTURE_SIZE
#define SOMEIP_DEFAULT_CALLBACK_CAPTURE_SIZE 32
#endif

#ifndef SOMEIP_MAX_TP_SEGMENTS
#define SOMEIP_MAX_TP_SEGMENTS 64
#endif

#ifndef SOMEIP_MAX_TCP_CONNECTIONS
#define SOMEIP_MAX_TCP_CONNECTIONS 10
#endif

#ifndef SOMEIP_MAX_TP_REASSEMBLY_SIZE
#define SOMEIP_MAX_TP_REASSEMBLY_SIZE 2048
#endif

#ifndef SOMEIP_MAX_SD_OPTIONS
#define SOMEIP_MAX_SD_OPTIONS 32
#endif

#ifndef SOMEIP_PIMPL_EVENTPUB_SIZE
#define SOMEIP_PIMPL_EVENTPUB_SIZE 65536
#endif

#ifndef SOMEIP_PIMPL_EVENTSUB_SIZE
#define SOMEIP_PIMPL_EVENTSUB_SIZE 36864
#endif

#ifndef SOMEIP_PIMPL_RPCCLIENT_SIZE
#define SOMEIP_PIMPL_RPCCLIENT_SIZE 32768
#endif

#ifndef SOMEIP_PIMPL_RPCSERVER_SIZE
#define SOMEIP_PIMPL_RPCSERVER_SIZE 32768
#endif

#ifndef SOMEIP_PIMPL_SDCLIENT_SIZE
#define SOMEIP_PIMPL_SDCLIENT_SIZE 65536
#endif

#ifndef SOMEIP_PIMPL_SDSERVER_SIZE
#define SOMEIP_PIMPL_SDSERVER_SIZE 32768
#endif

static_assert(SOMEIP_MESSAGE_POOL_SIZE > 0 &&
              SOMEIP_MESSAGE_POOL_SIZE <= 65535,
              "SOMEIP_MESSAGE_POOL_SIZE must fit in uint16_t (1..65535)");
static_assert(SOMEIP_BYTE_POOL_SMALL_COUNT > 0 &&
              SOMEIP_BYTE_POOL_SMALL_COUNT <= 65535,
              "SOMEIP_BYTE_POOL_SMALL_COUNT must fit in uint16_t (1..65535)");
static_assert(SOMEIP_BYTE_POOL_MEDIUM_COUNT > 0 &&
              SOMEIP_BYTE_POOL_MEDIUM_COUNT <= 65535,
              "SOMEIP_BYTE_POOL_MEDIUM_COUNT must fit in uint16_t (1..65535)");
static_assert(SOMEIP_BYTE_POOL_LARGE_COUNT > 0 &&
              SOMEIP_BYTE_POOL_LARGE_COUNT <= 65535,
              "SOMEIP_BYTE_POOL_LARGE_COUNT must fit in uint16_t (1..65535)");
static_assert(SOMEIP_BYTE_POOL_SMALL_SIZE > 0,
              "SOMEIP_BYTE_POOL_SMALL_SIZE must be positive");
static_assert(SOMEIP_BYTE_POOL_MEDIUM_SIZE > SOMEIP_BYTE_POOL_SMALL_SIZE,
              "SOMEIP_BYTE_POOL_MEDIUM_SIZE must exceed small tier size");
static_assert(SOMEIP_BYTE_POOL_LARGE_SIZE > SOMEIP_BYTE_POOL_MEDIUM_SIZE,
              "SOMEIP_BYTE_POOL_LARGE_SIZE must exceed medium tier size");

#endif // SOMEIP_PLATFORM_STATIC_CONFIG_H
