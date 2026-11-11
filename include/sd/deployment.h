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

#ifndef SOMEIP_SD_DEPLOYMENT_H
#define SOMEIP_SD_DEPLOYMENT_H

#include <cstddef>
#include <cstdint>

#include "common/result.h"
#include "sd/sd_types.h"

namespace someip::sd {

/**
 * @brief Services named in one list of a deployment document.
 *
 * Smaller than the default container cap so a stack `Deployment` fits in a
 * 4096-byte thread stack. A longer list fails the load with BUFFER_OVERFLOW.
 */
inline constexpr std::size_t DEPLOYMENT_MAX_SERVICES = 4;

/**
 * @brief Eventgroup ids named on one service.
 *
 * A longer list fails the load with BUFFER_OVERFLOW.
 */
inline constexpr std::size_t DEPLOYMENT_MAX_EVENTGROUPS = 8;

/**
 * @brief One service named by a deployment file.
 *
 * `instance` is the ServiceInstance the existing offer/find API already takes.
 * ServiceInstance has one port, so `unreliable_port` (UDP, protocol 0x11) and
 * `reliable_port` (TCP, protocol 0x06) are both kept. When the unreliable port
 * is non-zero, `instance.port` is that port. Otherwise `instance.port` is the
 * reliable port. Provided services copy `SdConfig::unicast_address` into
 * `instance.ip_address`. Required services leave the address empty; discovery
 * fills it.
 */
struct DeployedService {
    ServiceInstance instance;
    uint16_t unreliable_port{0};
    uint16_t reliable_port{0};
    platform::Vector<uint16_t, DEPLOYMENT_MAX_EVENTGROUPS> eventgroups;
};

/**
 * @brief Services and SD settings loaded from one deployment file.
 *
 * This is not a second configuration system. `sd` is an SdConfig. Each
 * service is a ServiceInstance plus the ports and eventgroup ids that
 * `SdServer::offer_service` and `SdClient::subscribe_eventgroup` already take.
 */
struct Deployment {
    SdConfig sd;
    platform::Vector<DeployedService, DEPLOYMENT_MAX_SERVICES> provided_services;
    platform::Vector<DeployedService, DEPLOYMENT_MAX_SERVICES> required_services;
};

/**
 * @brief Stack budget for an automatic Deployment plus the loader call.
 *
 * The supported automatic-call budget is 4096 bytes. A 2048-byte thread
 * cannot hold an automatic Deployment and call the loader. hello_s32k's
 * 2048-byte main stack is below that budget.
 *
 * The load path places the caller's Deployment, one automatic
 * DeployedService inside the service-array parse, and the parser's key
 * buffers on the stack. Reset clears that Deployment in place and does not
 * construct a second one. The file image is not an automatic variable. The
 * reserve is left for the caller.
 */
inline constexpr std::size_t DEPLOYMENT_STACK_BUDGET = 4096;
inline constexpr std::size_t DEPLOYMENT_CALLER_RESERVE = 1024;
inline constexpr std::size_t DEPLOYMENT_KEY_BUFFERS = 384;

static_assert(sizeof(Deployment) + sizeof(DeployedService) + DEPLOYMENT_KEY_BUFFERS +
                      DEPLOYMENT_CALLER_RESERVE <=
                  DEPLOYMENT_STACK_BUDGET,
              "an automatic Deployment does not fit in a 4096-byte stack");

/**
 * @brief Largest deployment document `load_deployment_file` will read.
 *
 * Parsing a larger file fails with BUFFER_OVERFLOW and does not keep a
 * partial result. The loader never opens a socket.
 */
inline constexpr std::size_t DEPLOYMENT_FILE_MAX_BYTES = 8192;

/**
 * @brief Parse a deployment document.
 *
 * The document is a single JSON object. No JSON library is vendored in this
 * tree, so only this object shape is accepted. Numbers are non-negative
 * integers (a leading zero is rejected). Identifiers and ports may instead be
 * strings, decimal or `0x` hex. Unknown keys fail. A missing required key
 * fails. The same service id and instance id may be provided once and
 * required once; a second copy in the same list fails. A provided service
 * needs a non-zero reliable or unreliable port.
 *
 * One leading UTF-8 BOM is skipped. A BOM anywhere else is malformed.
 * `multicast_address` must be non-empty. `unicast_address`, when present,
 * must be non-empty; when omitted, the SdConfig default is kept.
 * `ttl_ms` below 1000, `repetition_multiplier` of 0, `repetition_base_ms`
 * of 0, `repetition_max_ms` of 0, and `cyclic_offer_ms` of 0 are rejected.
 * `initial_delay_min_ms` of 0 is valid. `initial_delay_min_ms` greater than
 * `initial_delay_max_ms` is
 * rejected, as is a min/max pair whose inclusive span does not fit in a
 * uint32 millisecond count (0 and 4294967295).
 *
 * Required top-level keys: `sd`, `provided_services`, `required_services`.
 * Required `sd` keys: `multicast_address`, `multicast_port`,
 * `initial_delay_min_ms`, `initial_delay_max_ms`, `repetition_base_ms`,
 * `repetition_max_ms`, `repetition_multiplier`, `cyclic_offer_ms`, `ttl_ms`.
 * Optional `sd` keys: `unicast_address`, `unicast_port` (SdConfig defaults).
 * `initial_delay` is set from `initial_delay_max_ms` so the existing alias
 * matches the file. Required service keys: `service_id`, `instance_id`,
 * `major_version`, `minor_version`, `unreliable_port`, `reliable_port`,
 * `eventgroups`.
 *
 * On failure @p out is empty. `INVALID_ARGUMENT` is a schema rejection,
 * `MALFORMED_MESSAGE` is text that is not this JSON shape, and
 * `BUFFER_OVERFLOW` is a document, string, or service list past the static cap.
 */
Result load_deployment(const char* text, std::size_t length, Deployment& out);

/**
 * @brief Read @p path and parse it.
 *
 * The file image lives in static storage, not on the caller's stack and not
 * on the heap. This function does not open a socket and does not construct an
 * SD client or server. A missing path or a read error is `INVALID_ARGUMENT`.
 * A file longer than DEPLOYMENT_FILE_MAX_BYTES is `BUFFER_OVERFLOW` and is
 * not parsed.
 */
Result load_deployment_file(const char* path, Deployment& out);

}  // namespace someip::sd

#endif  // SOMEIP_SD_DEPLOYMENT_H
