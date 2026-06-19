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

#ifndef SOMEIP_PLATFORM_CONTAINERS_H
#define SOMEIP_PLATFORM_CONTAINERS_H

/**
 * @brief Portable container type aliases.
 *
 * The backend's containers_impl.h provides Vector, String, UnorderedMap,
 * Queue, and Function. The build system sets -I to the correct backend
 * directory (include/platform/static/ or include/platform/dynamic/).
 */

#include "containers_impl.h" // IWYU pragma: export

#endif // SOMEIP_PLATFORM_CONTAINERS_H
