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

#ifndef SOMEIP_PLATFORM_INTRUSIVE_PTR_H
#define SOMEIP_PLATFORM_INTRUSIVE_PTR_H

#include <cstddef>
#include <utility>

namespace someip::platform {

/**
 * @brief Intrusive reference-counting smart pointer.
 *
 * Requires ADL-visible free functions:
 *   void intrusive_ptr_add_ref(T const* p);
 *   void intrusive_ptr_release(T const* p);
 *
 * @implements REQ_PAL_INTRUSIVE_PTR
 */
template <typename T>
class IntrusivePtr {
public:
    IntrusivePtr() noexcept = default;

    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    IntrusivePtr(std::nullptr_t) noexcept {}

    explicit IntrusivePtr(T* p, bool add_ref = true) noexcept : ptr_(p) {
        if (ptr_ && add_ref) {
            intrusive_ptr_add_ref(ptr_);
        }
    }

    ~IntrusivePtr() {
        if (ptr_) {
            intrusive_ptr_release(ptr_);
        }
    }

    IntrusivePtr(const IntrusivePtr& o) noexcept : ptr_(o.ptr_) {
        if (ptr_) {
            intrusive_ptr_add_ref(ptr_);
        }
    }

    IntrusivePtr(IntrusivePtr&& o) noexcept : ptr_(o.ptr_) {
        o.ptr_ = nullptr;
    }

    IntrusivePtr& operator=(const IntrusivePtr& o) noexcept {
        if (this != &o) {
            IntrusivePtr(o).swap(*this);
        }
        return *this;
    }

    IntrusivePtr& operator=(IntrusivePtr&& o) noexcept {
        if (this != &o) {
            T* old = ptr_;
            ptr_ = o.ptr_;
            o.ptr_ = nullptr;
            if (old) {
                intrusive_ptr_release(old);
            }
        }
        return *this;
    }

    void reset() noexcept {
        IntrusivePtr().swap(*this);
    }

    void swap(IntrusivePtr& o) noexcept {
        T* tmp = ptr_;
        ptr_ = o.ptr_;
        o.ptr_ = tmp;
    }

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    bool operator==(const IntrusivePtr& o) const noexcept { return ptr_ == o.ptr_; }
    bool operator!=(const IntrusivePtr& o) const noexcept { return ptr_ != o.ptr_; }
    bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }
    bool operator!=(std::nullptr_t) const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_{nullptr};
};

}  // namespace someip::platform

#endif // SOMEIP_PLATFORM_INTRUSIVE_PTR_H
