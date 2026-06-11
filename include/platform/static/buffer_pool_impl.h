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

#ifndef SOMEIP_PLATFORM_STATIC_BUFFER_POOL_IMPL_H
#define SOMEIP_PLATFORM_STATIC_BUFFER_POOL_IMPL_H

#include "static_config.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <algorithm>

namespace someip::platform {

struct BufferSlot {
    uint8_t* data;
    size_t   capacity;
    size_t   size;
    uint8_t  tier;
    uint16_t index;
};

BufferSlot* acquire_buffer(size_t requested_size);
void        release_buffer(BufferSlot* slot);

/**
 * @brief Non-heap byte buffer backed by the tiered slab pool.
 *
 * Provides a std::vector<uint8_t>-compatible API so it can replace
 * std::vector<uint8_t> as a drop-in in message payloads and serialization
 * buffers without changing calling code.
 *
 * @implements REQ_PAL_BUFPOOL_ACQUIRE, REQ_PAL_BUFPOOL_RELEASE
 */
class ByteBuffer {
public:
    using value_type = uint8_t;
    using iterator = uint8_t*;
    using const_iterator = const uint8_t*;

    ByteBuffer() noexcept = default;

    ByteBuffer(std::initializer_list<uint8_t> init) {
        ensure_capacity(init.size());
        if (slot_) {
            std::memcpy(slot_->data, init.begin(), init.size());
            slot_->size = init.size();
        }
    }

    explicit ByteBuffer(size_t count, uint8_t value = 0) {
        ensure_capacity(count);
        if (slot_) {
            std::memset(slot_->data, value, count);
            slot_->size = count;
        }
    }

    ByteBuffer(const uint8_t* first, size_t count) {
        ensure_capacity(count);
        if (slot_ && first) {
            std::memcpy(slot_->data, first, count);
            slot_->size = count;
        }
    }

    ~ByteBuffer() {
        if (slot_) {
            release_buffer(slot_);
        }
    }

    ByteBuffer(ByteBuffer&& other) noexcept : slot_(other.slot_) {
        other.slot_ = nullptr;
    }

    ByteBuffer& operator=(ByteBuffer&& other) noexcept {
        if (this != &other) {
            if (slot_) {
                release_buffer(slot_);
            }
            slot_ = other.slot_;
            other.slot_ = nullptr;
        }
        return *this;
    }

    ByteBuffer(const ByteBuffer& other) {
        if (other.slot_ && other.slot_->size > 0) {
            ensure_capacity(other.slot_->size);
            if (slot_) {
                std::memcpy(slot_->data, other.slot_->data, other.slot_->size);
                slot_->size = other.slot_->size;
            }
        }
    }

    ByteBuffer& operator=(const ByteBuffer& other) {
        if (this != &other) {
            ByteBuffer tmp(other);
            std::swap(slot_, tmp.slot_);
        }
        return *this;
    }

    uint8_t*       data() noexcept { return slot_ ? slot_->data : nullptr; }
    const uint8_t* data() const noexcept { return slot_ ? slot_->data : nullptr; }
    size_t         size() const noexcept { return slot_ ? slot_->size : 0; }
    size_t         capacity() const noexcept { return slot_ ? slot_->capacity : 0; }
    bool           empty() const noexcept { return size() == 0; }

    void clear() noexcept {
        if (slot_) {
            slot_->size = 0;
        }
    }

    void resize(size_t new_size) {
        if (new_size == 0) {
            clear();
            return;
        }
        ensure_capacity(new_size);
        if (!slot_) { return; }
        if (new_size > slot_->size) {
            std::memset(slot_->data + slot_->size, 0, new_size - slot_->size);
        }
        slot_->size = new_size;
    }

    void resize(size_t new_size, uint8_t value) {
        if (new_size == 0) {
            clear();
            return;
        }
        size_t old_size = size();
        ensure_capacity(new_size);
        if (!slot_) { return; }
        if (new_size > old_size) {
            std::memset(slot_->data + old_size, value, new_size - old_size);
        }
        slot_->size = new_size;
    }

    void reserve(size_t min_capacity) {
        if (min_capacity > capacity()) {
            ensure_capacity(min_capacity);
        }
    }

    void push_back(uint8_t byte) {
        size_t cur = size();
        ensure_capacity(cur + 1);
        if (!slot_) { return; }
        slot_->data[cur] = byte;
        slot_->size = cur + 1;
    }

    void insert(const_iterator pos, const uint8_t* first, const uint8_t* last) {
        if (first == last) { return; }
        size_t insert_count = static_cast<size_t>(last - first);
        size_t offset = (pos && slot_) ? static_cast<size_t>(pos - slot_->data) : size();
        size_t new_size = size() + insert_count;
        ensure_capacity(new_size);
        if (!slot_) { return; }
        if (offset < slot_->size) {
            std::memmove(slot_->data + offset + insert_count,
                         slot_->data + offset,
                         slot_->size - offset);
        }
        std::memcpy(slot_->data + offset, first, insert_count);
        slot_->size = new_size;
    }

    uint8_t& operator[](size_t i) noexcept { return slot_->data[i]; }
    const uint8_t& operator[](size_t i) const noexcept { return slot_->data[i]; }

    iterator       begin() noexcept { return data(); }
    iterator       end() noexcept { return data() + size(); }
    const_iterator begin() const noexcept { return data(); }
    const_iterator end() const noexcept { return data() + size(); }

    bool operator==(const ByteBuffer& o) const noexcept {
        if (size() != o.size()) { return false; }
        return size() == 0 || std::memcmp(data(), o.data(), size()) == 0;
    }
    bool operator!=(const ByteBuffer& o) const noexcept { return !(*this == o); }

private:
    BufferSlot* slot_{nullptr};

    void ensure_capacity(size_t needed) {
        if (slot_ && slot_->capacity >= needed) { return; }
        BufferSlot* new_slot = acquire_buffer(needed);
        if (!new_slot) { return; }
        if (slot_) {
            if (slot_->size > 0) {
                std::memcpy(new_slot->data, slot_->data, slot_->size);
            }
            new_slot->size = slot_->size;
            release_buffer(slot_);
        } else {
            new_slot->size = 0;
        }
        slot_ = new_slot;
    }
};

}  // namespace someip::platform

#endif // SOMEIP_PLATFORM_STATIC_BUFFER_POOL_IMPL_H
