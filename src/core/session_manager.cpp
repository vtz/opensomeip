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

namespace someip {

/**
 * @brief Session Manager implementation
 * @implements REQ_ARCH_002
 * @implements REQ_ARCH_003
 *
 * Thread-safe session management using mutex protection.
 */

SessionManager::SessionManager() = default;

/**
 * @brief Create a new session
 * @implements REQ_ARCH_002
 */
uint16_t SessionManager::create_session(uint16_t client_id) {
    std::scoped_lock lock(sessions_mutex_);

    uint16_t session_id = get_next_session_id();

    auto session = std::make_shared<Session>(session_id, client_id);
    sessions_[session_id] = session;

    return session_id;
}

std::shared_ptr<Session> SessionManager::get_session(uint16_t session_id) {
    std::scoped_lock lock(sessions_mutex_);

    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }

    return nullptr;
}

void SessionManager::remove_session(uint16_t session_id) {
    std::scoped_lock lock(sessions_mutex_);

    sessions_.erase(session_id);
}

bool SessionManager::validate_session(uint16_t session_id) {
    std::scoped_lock lock(sessions_mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }

    return it->second->state == SessionState::ACTIVE;
}

void SessionManager::update_session_activity(uint16_t session_id) {
    std::scoped_lock lock(sessions_mutex_);

    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second->update_activity();
    }
}

size_t SessionManager::cleanup_expired_sessions(std::chrono::seconds timeout) {
    std::scoped_lock lock(sessions_mutex_);

    size_t cleaned_count = 0;
    auto it = sessions_.begin();

    while (it != sessions_.end()) {
        if (it->second->is_expired(timeout)) {
            it->second->state = SessionState::EXPIRED;
            it = sessions_.erase(it);
            cleaned_count++;
        } else {
            ++it;
        }
    }

    return cleaned_count;
}

uint16_t SessionManager::get_next_session_id() {
    // Find the next available session ID for this client
    // SOME/IP session IDs should not be 0
    uint16_t candidate = next_session_id_;

    // Simple linear search for available ID (could be optimized)
    while (sessions_.find(candidate) != sessions_.end()) {
        candidate++;
        if (candidate == 0) { // Wrap around, skip 0
            candidate = 1;
        }
    }

    next_session_id_ = candidate + 1;
    if (next_session_id_ == 0) {
        next_session_id_ = 1;
    }

    return candidate;
}

size_t SessionManager::get_active_session_count() const {
    std::scoped_lock lock(sessions_mutex_);

    size_t count = 0;
    for (const auto& pair : sessions_) {
        if (pair.second->state == SessionState::ACTIVE) {
            count++;
        }
    }

    return count;
}

} // namespace someip
