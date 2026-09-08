/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_TESTS_STATIC_POOL_INIT_H
#define SOMEIP_TESTS_STATIC_POOL_INIT_H

#ifdef SOMEIP_STATIC_ALLOC

#include <gtest/gtest.h>
#include "platform/memory.h"

namespace someip::test {

class StaticPoolEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        someip::platform::init_static_allocator();
    }
};

[[maybe_unused]] static auto* const g_static_pool_env =
    ::testing::AddGlobalTestEnvironment(new StaticPoolEnvironment());

}  // namespace someip::test

#endif  // SOMEIP_STATIC_ALLOC

#endif  // SOMEIP_TESTS_STATIC_POOL_INIT_H
