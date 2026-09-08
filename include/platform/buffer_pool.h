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

#ifndef SOMEIP_PLATFORM_BUFFER_POOL_H
#define SOMEIP_PLATFORM_BUFFER_POOL_H

/**
 * @brief Portable byte-buffer pool types.
 *
 * The backend's buffer_pool_impl.h provides ByteBuffer and pool accessors.
 * The build system sets -I to the correct backend directory.
 */

#include "buffer_pool_impl.h" // IWYU pragma: export

#endif // SOMEIP_PLATFORM_BUFFER_POOL_H
