/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_STATIC_CONTAINERS_IMPL_H
#define SOMEIP_PLATFORM_STATIC_CONTAINERS_IMPL_H

/**
 * @brief Static (no-heap) container type aliases backed by ETL.
 *
 * Default capacities come from static_config.h and may be overridden via -D.
 */

#include "static_config.h"

#include <etl/inplace_function.h>
#include <etl/queue.h>
#include <etl/string.h>
#include <etl/unordered_map.h>
#include <etl/vector.h>

#include <cstddef>

namespace someip::platform {

template <typename T, std::size_t N = SOMEIP_DEFAULT_VECTOR_CAPACITY>
using Vector = etl::vector<T, N>;

template <std::size_t N = SOMEIP_DEFAULT_STRING_CAPACITY>
using String = etl::string<N>;

template <typename K, typename V, std::size_t N = SOMEIP_DEFAULT_MAP_CAPACITY>
using UnorderedMap = etl::unordered_map<K, V, N>;

template <typename T, std::size_t N = SOMEIP_DEFAULT_QUEUE_CAPACITY>
using Queue = etl::queue<T, N>;

template <typename Sig, std::size_t N = SOMEIP_DEFAULT_CALLBACK_CAPTURE_SIZE>
using Function = etl::inplace_function<Sig, N>;

}  // namespace someip::platform

#endif // SOMEIP_PLATFORM_STATIC_CONTAINERS_IMPL_H
