/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PAYLOAD_VIEW_H
#define SOMEIP_PAYLOAD_VIEW_H

/**
 * @brief Non-owning view over a contiguous byte range.
 *
 * PayloadView provides span-like semantics for zero-copy payload access
 * without requiring C++20's std::span. Works identically with both
 * dynamic (std::vector) and static (slab-backed ByteBuffer) payloads.
 *
 * @implements REQ_API_PAYLOAD_VIEW
 */

#include <cstddef>
#include <cstdint>

namespace someip {

class PayloadView {
public:
    constexpr PayloadView() noexcept = default;

    constexpr PayloadView(const uint8_t* data, size_t size) noexcept
        : data_(data), size_(size) {}

    template <typename Container>
    explicit PayloadView(const Container& c) noexcept
        : data_(c.data()), size_(c.size()) {}

    [[nodiscard]] constexpr const uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] constexpr size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr const uint8_t& operator[](size_t i) const noexcept {
        return data_[i];
    }

    [[nodiscard]] constexpr const uint8_t* begin() const noexcept { return data_; }
    [[nodiscard]] constexpr const uint8_t* end() const noexcept { return data_ + size_; }

    [[nodiscard]] constexpr PayloadView subview(size_t offset, size_t count) const noexcept {
        if (offset >= size_) { return {}; }
        if (count > size_ - offset) { count = size_ - offset; }
        return {data_ + offset, count};
    }

private:
    const uint8_t* data_{nullptr};
    size_t size_{0};
};

}  // namespace someip

#endif // SOMEIP_PAYLOAD_VIEW_H
