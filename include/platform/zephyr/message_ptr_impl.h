/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_ZEPHYR_MESSAGE_PTR_IMPL_H
#define SOMEIP_PLATFORM_ZEPHYR_MESSAGE_PTR_IMPL_H

#include <memory>

namespace someip {
class Message;
using MessagePtr = std::shared_ptr<Message>;
using MessageConstPtr = std::shared_ptr<const Message>;
}  // namespace someip

#endif // SOMEIP_PLATFORM_ZEPHYR_MESSAGE_PTR_IMPL_H
