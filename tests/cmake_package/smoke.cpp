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

#include "someip/message.h"
#include "someip/types.h"

int main() {
    someip::Message msg(someip::MessageId(0x1234, 0x0001),
                        someip::RequestId(0x0001, 0x0001));
    return msg.has_valid_header() ? 0 : 1;
}
