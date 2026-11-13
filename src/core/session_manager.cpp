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

#include "core/session_manager.h"

#include "platform/thread.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace someip {

/**
 * @brief Session Manager implementation
 *
 * Thread-safe session management using mutex protection.
 */

SessionManager::SessionManager() = default;

/**
 * @brief Create a new session
 * @implements REQ_ARCH_002
 * @implements REQ_ARCH_003
 * @implements REQ_MSG_118
 */
uint16_t SessionManager::create_session(uint16_t client_id) {
    platform::ScopedLock const lock(sessions_mutex_);

    if (sessions_.size() >= sessions_.max_size()) {
        return 0;
    }

    uint16_t const session_id = get_next_session_id();
    // 0 means the 16-bit space is exhausted. Do not insert a reserved ID.
    if (session_id == 0) {
        return 0;
    }
    sessions_.insert({session_id, Session(session_id, client_id)});

    return session_id;
}

std::optional<Session> SessionManager::get_session(uint16_t session_id) {
    platform::ScopedLock const lock(sessions_mutex_);

    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool SessionManager::set_session_state(uint16_t session_id, SessionState state) {
    platform::ScopedLock const lock(sessions_mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }
    it->second.state = state;
    return true;
}

void SessionManager::remove_session(uint16_t session_id) {
    platform::ScopedLock const lock(sessions_mutex_);

    sessions_.erase(session_id);
}

bool SessionManager::validate_session(uint16_t session_id) {
    platform::ScopedLock const lock(sessions_mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }

    return it->second.state == SessionState::ACTIVE;
}

void SessionManager::update_session_activity(uint16_t session_id) {
    platform::ScopedLock const lock(sessions_mutex_);

    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.update_activity();
    }
}

size_t SessionManager::cleanup_expired_sessions(std::chrono::steady_clock::duration timeout) {
    platform::ScopedLock const lock(sessions_mutex_);

    size_t cleaned_count = 0;
    auto it = sessions_.begin();

    while (it != sessions_.end()) {
        if (it->second.is_expired(timeout)) {
            it->second.state = SessionState::EXPIRED;
            it = sessions_.erase(it);
            cleaned_count++;
        } else {
            ++it;
        }
    }

    return cleaned_count;
}

uint16_t SessionManager::get_next_session_id() {
    // Session id 0 is reserved. Usable ids are 1..65535. A table that already
    // holds every usable id returns 0; searching until one is free would not finish.
    constexpr uint16_t first_session_id = 1;
    constexpr uint32_t usable_session_id_count = 65535U;
    if (sessions_.size() >= usable_session_id_count) {
        return 0;
    }

    uint16_t candidate = next_session_id_;
    if (candidate == 0) {
        candidate = first_session_id;
    }

    for (uint32_t scanned = 0; scanned < usable_session_id_count; ++scanned) {
        if (sessions_.find(candidate) == sessions_.end()) {
            const uint32_t advanced = static_cast<uint32_t>(candidate) + 1U;
            next_session_id_ = (advanced > usable_session_id_count)
                                   ? first_session_id
                                   : static_cast<uint16_t>(advanced);
            return candidate;
        }
        const uint32_t advanced = static_cast<uint32_t>(candidate) + 1U;
        candidate = (advanced > usable_session_id_count) ? first_session_id
                                                         : static_cast<uint16_t>(advanced);
    }
    return 0;
}

size_t SessionManager::get_active_session_count() const {
    platform::ScopedLock const lock(sessions_mutex_);

    size_t count = 0;
    for (const auto& pair : sessions_) {
        if (pair.second.state == SessionState::ACTIVE) {
            count++;
        }
    }

    return count;
}

size_t SessionManager::session_count() const {
    platform::ScopedLock const lock(sessions_mutex_);
    return sessions_.size();
}

} // namespace someip
