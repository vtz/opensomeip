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

#ifndef OPENSOMEIP_CAPI_INTERNAL_H
#define OPENSOMEIP_CAPI_INTERNAL_H

#include "someip/message.h"

struct opensomeip_message_s {
    someip::Message msg;
};

#endif /* OPENSOMEIP_CAPI_INTERNAL_H */
