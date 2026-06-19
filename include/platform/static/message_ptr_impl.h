/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_STATIC_MESSAGE_PTR_IMPL_H
#define SOMEIP_PLATFORM_STATIC_MESSAGE_PTR_IMPL_H

#include "platform/intrusive_ptr.h"

namespace someip {

class Message;

using MessagePtr = platform::IntrusivePtr<Message>;
using MessageConstPtr = platform::IntrusivePtr<const Message>;

}  // namespace someip

#endif // SOMEIP_PLATFORM_STATIC_MESSAGE_PTR_IMPL_H
