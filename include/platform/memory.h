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

#ifndef SOMEIP_PLATFORM_MEMORY_H
#define SOMEIP_PLATFORM_MEMORY_H

/**
 * @brief Portable memory management for Message objects.
 *
 * The backend's memory_impl.h provides allocate_message() and
 * optionally release_message(). The build system sets -I to the
 * correct backend directory.
 */

#include "someip/message.h"
#include <memory>

#include "memory_impl.h"

#endif // SOMEIP_PLATFORM_MEMORY_H
