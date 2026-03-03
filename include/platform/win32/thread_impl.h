/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_WIN32_THREAD_IMPL_H
#define SOMEIP_PLATFORM_WIN32_THREAD_IMPL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "platform/host/host_condition_variable.h"

namespace someip {
namespace platform {

using Thread = std::thread;
using Mutex = std::mutex;

namespace this_thread {
using std::this_thread::sleep_for;
} // namespace this_thread

} // namespace platform
} // namespace someip

#endif // SOMEIP_PLATFORM_WIN32_THREAD_IMPL_H
