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

#ifndef SOMEIP_PLATFORM_NET_H
#define SOMEIP_PLATFORM_NET_H

/**
 * @brief Portable socket and network header inclusion.
 *
 * All transport code should include this header instead of
 * directly including platform-specific network headers.
 * The backend's net_impl.h provides socket includes, macro
 * mappings, and portable helpers (someip_close_socket, etc.).
 */

#include "net_impl.h"

#endif // SOMEIP_PLATFORM_NET_H
