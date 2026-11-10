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
    platform::Vector<uint16_t> eventgroups;
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
    platform::Vector<DeployedService> provided_services;
    platform::Vector<DeployedService> required_services;
};

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
 * `BUFFER_OVERFLOW` is a document or string past the static cap.
 */
Result load_deployment(const char* text, std::size_t length, Deployment& out);

/**
 * @brief Read @p path and parse it.
 *
 * The file is read into a fixed buffer. This function does not open a socket
 * and does not construct an SD client or server. A missing path is
 * `INVALID_ARGUMENT`.
 */
Result load_deployment_file(const char* path, Deployment& out);

}  // namespace someip::sd

#endif  // SOMEIP_SD_DEPLOYMENT_H
