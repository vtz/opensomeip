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

#ifndef E2E_BASIC_PROFILE_H
#define E2E_BASIC_PROFILE_H

#include "e2e/e2e_profile.h"

namespace someip {
namespace e2e {

/**
 * @brief Initialize and register the basic E2E profile
 *
 * This function registers the basic E2E protection profile, a simple reference
 * implementation using publicly available standards (SAE-J1850, ITU-T X.25).
 *
 * IMPORTANT: This is NOT an industry standard E2E profile. It provides basic
 * E2E protection mechanisms for testing and development purposes only.
 *
 * For production use in AUTOSAR environments, implement AUTOSAR E2E profiles
 * (P01, P02, P04, P05, P06, P07, P11) as external plugins due to licensing
 * restrictions.
 *
 * Should be called during library initialization.
 */
void initialize_basic_profile();

} // namespace e2e
} // namespace someip

#endif // E2E_BASIC_PROFILE_H
