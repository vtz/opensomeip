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

#include <gtest/gtest.h>
#include "core/session_manager.h"
#include <thread>
#include <chrono>
#include <unordered_set>

using namespace someip;

/**
 * @brief Session Manager unit tests
 *
 * Tests cover session lifecycle, ID generation, state transitions,
 * expiry, and edge cases. MC/DC coverage is applied to the compound
 * decisions in validate_session, cleanup_expired_sessions, and
 * get_next_session_id.
 *
 * @tests REQ_ARCH_002
 * @tests REQ_ARCH_003
 * @tests REQ_MSG_118
 */
class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        session_mgr_ = std::make_unique<SessionManager>();
    }

    void TearDown() override {
        session_mgr_.reset();
    }

    std::unique_ptr<SessionManager> session_mgr_;
};

// ============================================================================
// Basic session creation
// ============================================================================

TEST_F(SessionManagerTest, CreateSession) {
    uint16_t client_id = 0x1001;
    uint16_t session_id = session_mgr_->create_session(client_id);

    EXPECT_NE(session_id, 0u);
    EXPECT_TRUE(session_mgr_->validate_session(session_id));
}

TEST_F(SessionManagerTest, MultipleClients) {
    uint16_t client1 = 0x1001;
    uint16_t client2 = 0x1002;

    uint16_t session1 = session_mgr_->create_session(client1);
    uint16_t session2 = session_mgr_->create_session(client2);

    EXPECT_TRUE(session_mgr_->validate_session(session1));
    EXPECT_TRUE(session_mgr_->validate_session(session2));
    EXPECT_NE(session1, session2);
}

TEST_F(SessionManagerTest, SessionNotFound) {
    EXPECT_FALSE(session_mgr_->validate_session(9999));
}

TEST_F(SessionManagerTest, GetSessionInfo) {
    uint16_t client_id = 0x1001;
    uint16_t session_id = session_mgr_->create_session(client_id);

    auto session = session_mgr_->get_session(session_id);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->session_id, session_id);
    EXPECT_EQ(session->client_id, client_id);
    EXPECT_EQ(session->state, SessionState::ACTIVE);
}

// ============================================================================
// Session removal and invalidation
// ============================================================================

/**
 * @test_case TC_SM_REMOVE_001
 * @tests REQ_ARCH_002
 * @brief remove_session makes session invalid and unreachable
 */
TEST_F(SessionManagerTest, RemoveSession) {
    uint16_t session_id = session_mgr_->create_session(0x1001);
    ASSERT_TRUE(session_mgr_->validate_session(session_id));

    session_mgr_->remove_session(session_id);
    EXPECT_FALSE(session_mgr_->validate_session(session_id));
    EXPECT_EQ(session_mgr_->get_session(session_id), nullptr);
}

/**
 * @test_case TC_SM_REMOVE_002
 * @tests REQ_ARCH_002
 * @brief remove_session on nonexistent ID is safe (no-op)
 */
TEST_F(SessionManagerTest, RemoveNonexistentSession) {
    session_mgr_->remove_session(9999);
    EXPECT_EQ(session_mgr_->get_active_session_count(), 0u);
}

// ============================================================================
// get_session edge cases — MC/DC for (it == sessions_.end())
// ============================================================================

/**
 * @test_case TC_SM_GET_001
 * @tests REQ_ARCH_002
 * @brief get_session returns nullptr for nonexistent session
 */
TEST_F(SessionManagerTest, GetSession_InvalidId) {
    EXPECT_EQ(session_mgr_->get_session(0), nullptr);
    EXPECT_EQ(session_mgr_->get_session(0xFFFF), nullptr);
}

// ============================================================================
// Same client, multiple sessions
// ============================================================================

/**
 * @test_case TC_SM_MULTI_001
 * @tests REQ_ARCH_002
 * @brief Same client can create multiple sessions with unique IDs
 */
TEST_F(SessionManagerTest, SameClientMultipleSessions) {
    uint16_t client_id = 0x1001;

    uint16_t s1 = session_mgr_->create_session(client_id);
    uint16_t s2 = session_mgr_->create_session(client_id);
    uint16_t s3 = session_mgr_->create_session(client_id);

    EXPECT_NE(s1, s2);
    EXPECT_NE(s2, s3);
    EXPECT_NE(s1, s3);

    EXPECT_TRUE(session_mgr_->validate_session(s1));
    EXPECT_TRUE(session_mgr_->validate_session(s2));
    EXPECT_TRUE(session_mgr_->validate_session(s3));

    auto sess = session_mgr_->get_session(s2);
    ASSERT_NE(sess, nullptr);
    EXPECT_EQ(sess->client_id, client_id);
}

// ============================================================================
// Session state and activity
// ============================================================================

/**
 * @test_case TC_SM_STATE_001
 * @tests REQ_ARCH_002
 * @brief validate_session returns false for INACTIVE session
 *
 * MC/DC for: it->second->state == SessionState::ACTIVE
 *   state==ACTIVE → true
 *   state!=ACTIVE → false
 */
TEST_F(SessionManagerTest, ValidateSession_InactiveState) {
    uint16_t session_id = session_mgr_->create_session(0x1001);

    auto session = session_mgr_->get_session(session_id);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(session_mgr_->validate_session(session_id));

    session->state = SessionState::INACTIVE;
    EXPECT_FALSE(session_mgr_->validate_session(session_id));
}

/**
 * @test_case TC_SM_STATE_002
 * @tests REQ_ARCH_002
 * @brief validate_session returns false for ERROR state
 */
TEST_F(SessionManagerTest, ValidateSession_ErrorState) {
    uint16_t session_id = session_mgr_->create_session(0x1001);

    auto session = session_mgr_->get_session(session_id);
    session->state = SessionState::ERROR;
    EXPECT_FALSE(session_mgr_->validate_session(session_id));
}

/**
 * @test_case TC_SM_ACTIVITY_001
 * @tests REQ_ARCH_002
 * @brief update_session_activity refreshes last_activity timestamp
 */
TEST_F(SessionManagerTest, UpdateActivity) {
    uint16_t session_id = session_mgr_->create_session(0x1001);
    auto session = session_mgr_->get_session(session_id);
    auto original_time = session->last_activity;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    session_mgr_->update_session_activity(session_id);

    EXPECT_GT(session->last_activity, original_time);
}

/**
 * @test_case TC_SM_ACTIVITY_002
 * @tests REQ_ARCH_002
 * @brief update_session_activity on nonexistent ID is safe
 */
TEST_F(SessionManagerTest, UpdateActivity_InvalidId) {
    session_mgr_->update_session_activity(9999);
    EXPECT_EQ(session_mgr_->get_active_session_count(), 0u);
}

// ============================================================================
// Expiry
// ============================================================================

TEST_F(SessionManagerTest, CleanupExpiredSessions) {
    uint16_t client_id = 0x1001;
    uint16_t session_id = session_mgr_->create_session(client_id);

    size_t cleaned = session_mgr_->cleanup_expired_sessions(std::chrono::seconds(0));
    EXPECT_EQ(cleaned, 1u);
    EXPECT_FALSE(session_mgr_->validate_session(session_id));
}

/**
 * @test_case TC_SM_EXPIRY_001
 * @tests REQ_ARCH_002
 * @brief Sessions within timeout are NOT cleaned up
 *
 * MC/DC for is_expired: (now - last_activity) >= timeout
 *   diff >= timeout → expired=true → cleaned
 *   diff < timeout  → expired=false → kept
 */
TEST_F(SessionManagerTest, CleanupKeepsFreshSessions) {
    session_mgr_->create_session(0x1001);
    session_mgr_->create_session(0x1002);

    size_t cleaned = session_mgr_->cleanup_expired_sessions(std::chrono::seconds(3600));
    EXPECT_EQ(cleaned, 0u);
    EXPECT_EQ(session_mgr_->get_active_session_count(), 2u);
}

/**
 * @test_case TC_SM_EXPIRY_002
 * @tests REQ_ARCH_002
 * @brief Mixed fresh and expired sessions: only stale ones removed
 */
TEST_F(SessionManagerTest, CleanupMixedSessions) {
    uint16_t s1 = session_mgr_->create_session(0x1001);
    uint16_t s2 = session_mgr_->create_session(0x1002);

    auto stale = session_mgr_->get_session(s1);
    auto fresh = session_mgr_->get_session(s2);
    ASSERT_NE(stale, nullptr);
    ASSERT_NE(fresh, nullptr);

    // Artificially age s1 by shifting its last_activity back
    stale->last_activity -= std::chrono::seconds(2);

    // Timeout of 1s: s1 (2s old) should be expired, s2 (fresh) should not
    size_t cleaned = session_mgr_->cleanup_expired_sessions(std::chrono::seconds(1));
    EXPECT_EQ(cleaned, 1u);
    EXPECT_FALSE(session_mgr_->validate_session(s1));
    EXPECT_TRUE(session_mgr_->validate_session(s2));
}

// ============================================================================
// Session count
// ============================================================================

TEST_F(SessionManagerTest, SessionCount) {
    EXPECT_EQ(session_mgr_->get_active_session_count(), 0u);

    session_mgr_->create_session(0x1001);
    session_mgr_->create_session(0x1002);
    EXPECT_EQ(session_mgr_->get_active_session_count(), 2u);

    session_mgr_->cleanup_expired_sessions(std::chrono::seconds(0));
    EXPECT_EQ(session_mgr_->get_active_session_count(), 0u);
}

/**
 * @test_case TC_SM_COUNT_001
 * @tests REQ_ARCH_002
 * @brief get_active_session_count only counts ACTIVE sessions
 */
TEST_F(SessionManagerTest, SessionCount_ExcludesNonActive) {
    uint16_t s1 = session_mgr_->create_session(0x1001);
    session_mgr_->create_session(0x1002);

    EXPECT_EQ(session_mgr_->get_active_session_count(), 2u);

    auto session = session_mgr_->get_session(s1);
    session->state = SessionState::INACTIVE;

    EXPECT_EQ(session_mgr_->get_active_session_count(), 1u);
}

// ============================================================================
// Session struct direct tests
// ============================================================================

/**
 * @test_case TC_SM_STRUCT_001
 * @tests REQ_ARCH_002
 * @brief Session default constructor initializes fields
 */
TEST_F(SessionManagerTest, SessionDefaultConstruction) {
    Session s;
    EXPECT_EQ(s.session_id, 0u);
    EXPECT_EQ(s.client_id, 0u);
    EXPECT_EQ(s.state, SessionState::ACTIVE);
}

/**
 * @test_case TC_SM_STRUCT_002
 * @tests REQ_ARCH_002
 * @brief Session parameterized constructor sets IDs
 */
TEST_F(SessionManagerTest, SessionParameterizedConstruction) {
    Session s(42, 0x1234);
    EXPECT_EQ(s.session_id, 42u);
    EXPECT_EQ(s.client_id, 0x1234u);
    EXPECT_EQ(s.state, SessionState::ACTIVE);
}

/**
 * @test_case TC_SM_STRUCT_003
 * @tests REQ_ARCH_002
 * @brief Session::is_expired is true when elapsed >= timeout
 *
 * MC/DC for: (now - last_activity) >= timeout
 *   elapsed >= timeout → true   (timeout=0 guarantees elapsed >= 0)
 *   elapsed < timeout  → false  (timeout=3600s, session just created)
 *
 * The exact boundary case (elapsed == timeout) is intentionally omitted
 * because wall-clock timing makes precise equality tests unreliable in
 * unit tests. The two cases above achieve the required decision coverage.
 */
TEST_F(SessionManagerTest, Session_IsExpired) {
    Session s(1, 0x1001);

    EXPECT_FALSE(s.is_expired(std::chrono::seconds(3600)));
    EXPECT_TRUE(s.is_expired(std::chrono::seconds(0)));
}

/**
 * @test_case TC_SM_STRUCT_004
 * @tests REQ_ARCH_002
 * @brief Session::update_activity refreshes timestamp
 */
TEST_F(SessionManagerTest, Session_UpdateActivity) {
    Session s(1, 0x1001);
    auto t1 = s.last_activity;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    s.update_activity();

    EXPECT_GT(s.last_activity, t1);
}

// ============================================================================
// Session ID generation edge cases
// ============================================================================

/**
 * @test_case TC_SM_IDGEN_001
 * @tests REQ_MSG_118
 * @brief Sequential session IDs are unique and nonzero
 */
TEST_F(SessionManagerTest, SessionIdGeneration_NeverZero) {
    for (int i = 0; i < 100; ++i) {
        uint16_t sid = session_mgr_->create_session(0x1001);
        EXPECT_NE(sid, 0u) << "Session ID must never be 0 (SOME/IP spec)";
    }
}

/**
 * @test_case TC_SM_IDGEN_002
 * @tests REQ_MSG_118
 * @brief Session IDs are unique across many creations
 */
TEST_F(SessionManagerTest, SessionIdGeneration_Uniqueness) {
    std::unordered_set<uint16_t> ids;
    for (int i = 0; i < 200; ++i) {
        uint16_t sid = session_mgr_->create_session(0x1001);
        auto [_, inserted] = ids.insert(sid);
        EXPECT_TRUE(inserted) << "Duplicate session ID: " << sid;
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
