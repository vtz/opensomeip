/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

int main() {
    printf("=== SOME/IP Stack - S32K Platform Test ===\n");

    std::string name = "SOME/IP on S32K";
    printf("String: %s (len=%zu)\n", name.c_str(), name.size());

    std::vector<uint8_t> buf = {0xDE, 0xAD, 0xBE, 0xEF};
    printf("Vector: %zu bytes: ", buf.size());
    for (auto b : buf) {
        printf("0x%02X ", b);
    }
    printf("\n");

    auto ptr = std::make_shared<int>(42);
    printf("shared_ptr value: %d (use_count=%ld)\n", *ptr, ptr.use_count());

    printf("=== C++17 STL on embedded: OK ===\n");
    return 0;
}
