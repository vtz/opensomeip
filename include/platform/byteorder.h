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

#ifndef SOMEIP_PLATFORM_BYTEORDER_H
#define SOMEIP_PLATFORM_BYTEORDER_H

/**
 * @brief Portable byte-order conversion macros.
 *
 * The backend's byteorder_impl.h defines someip_htons, someip_ntohs,
 * someip_htonl, someip_ntohl. The build system sets -I to the correct
 * backend directory.
 */

#include "byteorder_impl.h"

#endif // SOMEIP_PLATFORM_BYTEORDER_H
