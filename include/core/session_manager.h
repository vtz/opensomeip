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

#ifndef SOMEIP_CORE_SESSION_MANAGER_H
#define SOMEIP_CORE_SESSION_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "platform/thread.h"
#include <chrono>

namespace someip {

/**
 * @brief Session state enumeration
 */
enum class SessionState : uint8_t {
    ACTIVE,
    INACTIVE,
    EXPIRED,
    ERROR
};

/**
 * @brief Session information
 */
struct Session {
    uint16_t session_id{0};
    uint16_t client_id{0};
    std::chrono::steady_clock::time_point last_activity{std::chrono::steady_clock::now()};
    SessionState state{SessionState::ACTIVE};

    Session() = default;
    Session(uint16_t sid, uint16_t cid)
        : session_id(sid), client_id(cid) {}

    void update_activity() {
        last_activity = std::chrono::steady_clock::now();
    }

    bool is_expired(std::chrono::seconds timeout) const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_activity) > timeout;
    }
};

// Forward declaration
class SessionManager;

/**
 * @brief Session manager for managing client sessions
 *
 * This class manages client sessions for SOME/IP communication,
 * ensuring unique session IDs and proper session lifecycle.
 */
class SessionManager {
public:
    /**
     * @brief Constructor
     */
    SessionManager();

    /**
     * @brief Destructor
     */
    ~SessionManager() = default;

    /**
     * @brief Create a new session for a client
     * @param client_id The client ID
     * @return New session ID
     */
    uint16_t create_session(uint16_t client_id);

    /**
     * @brief Get session information
     * @param session_id The session ID to look up
     * @return Pointer to session or nullptr if not found
     */
    std::shared_ptr<Session> get_session(uint16_t session_id);

    /**
     * @brief Remove a session
     * @param session_id The session ID to remove
     */
    void remove_session(uint16_t session_id);

    /**
     * @brief Validate if a session is active
     * @param session_id The session ID to validate
     * @return true if session is valid and active
     */
    bool validate_session(uint16_t session_id);

    /**
     * @brief Update session activity timestamp
     * @param session_id The session ID to update
     */
    void update_session_activity(uint16_t session_id);

    /**
     * @brief Clean up expired sessions
     * @param timeout Timeout duration for session expiry
     * @return Number of sessions cleaned up
     */
    size_t cleanup_expired_sessions(std::chrono::seconds timeout);

    /**
     * @brief Get the next available session ID
     * @param client_id The client ID
     * @return Next available session ID for the client
     */
    uint16_t get_next_session_id();

    /**
     * @brief Get number of active sessions
     * @return Number of active sessions
     */
    size_t get_active_session_count() const;

private:
    std::unordered_map<uint16_t, std::shared_ptr<Session>> sessions_;
    mutable platform::Mutex sessions_mutex_;
    uint16_t next_session_id_{1};

    // Prevent copying
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
};

} // namespace someip

#endif // SOMEIP_CORE_SESSION_MANAGER_H
