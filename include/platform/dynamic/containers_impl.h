/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_DYNAMIC_CONTAINERS_IMPL_H
#define SOMEIP_PLATFORM_DYNAMIC_CONTAINERS_IMPL_H

/**
 * @brief Dynamic (heap-backed) container type aliases.
 *
 * Template parameter N is accepted for API compatibility with the static
 * backend but is ignored.
 */

#include <cstddef>
#include <functional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace someip::platform {

template <typename T, std::size_t N = 0>
using Vector = std::vector<T>;

template <std::size_t N = 0>
using String = std::string;

template <typename K, typename V, std::size_t N = 0, typename Hash = std::hash<K>>
using UnorderedMap = std::unordered_map<K, V, Hash>;

template <typename T, std::size_t N = 0>
using Queue = std::queue<T>;

template <typename Sig, std::size_t N = 0>
using Function = std::function<Sig>;

}  // namespace someip::platform

#endif // SOMEIP_PLATFORM_DYNAMIC_CONTAINERS_IMPL_H
