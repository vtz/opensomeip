/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_MESSAGE_PTR_H
#define SOMEIP_PLATFORM_MESSAGE_PTR_H

/**
 * @brief Dispatch header for the platform-specific MessagePtr alias.
 *
 * The build system sets -I to the correct backend directory so that the
 * compiler finds the appropriate message_ptr_impl.h:
 *   - dynamic/  → std::shared_ptr<Message>
 *   - static/   → platform::IntrusivePtr<Message>
 */

#include "message_ptr_impl.h" // IWYU pragma: export

#endif // SOMEIP_PLATFORM_MESSAGE_PTR_H
