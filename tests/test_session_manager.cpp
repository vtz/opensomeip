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

using namespace someip;

/**
 * @brief Session Manager unit tests
 * @tests REQ_ARCH_002
 * @tests REQ_ARCH_003
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

TEST_F(SessionManagerTest, CreateSession) {
    uint16_t client_id = 0x1001;
    uint16_t session_id = session_mgr_->create_session(client_id);

    EXPECT_NE(session_id, 0); // Session ID should not be 0
    EXPECT_TRUE(session_mgr_->validate_session(session_id));
}

TEST_F(SessionManagerTest, MultipleClients) {
    uint16_t client1 = 0x1001;
    uint16_t client2 = 0x1002;

    uint16_t session1 = session_mgr_->create_session(client1);
    uint16_t session2 = session_mgr_->create_session(client2);

    EXPECT_TRUE(session_mgr_->validate_session(session1));
    EXPECT_TRUE(session_mgr_->validate_session(session2));
    EXPECT_NE(session1, session2); // Different clients should get different sessions
}

TEST_F(SessionManagerTest, SessionNotFound) {
    EXPECT_FALSE(session_mgr_->validate_session(9999)); // Non-existent session
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

TEST_F(SessionManagerTest, CleanupExpiredSessions) {
    uint16_t client_id = 0x1001;
    uint16_t session_id = session_mgr_->create_session(client_id);

    // Cleanup with 0 timeout should remove all sessions
    size_t cleaned = session_mgr_->cleanup_expired_sessions(std::chrono::seconds(0));
    EXPECT_EQ(cleaned, 1);
    EXPECT_FALSE(session_mgr_->validate_session(session_id));
}

TEST_F(SessionManagerTest, SessionCount) {
    EXPECT_EQ(session_mgr_->get_active_session_count(), 0);

    session_mgr_->create_session(0x1001);
    session_mgr_->create_session(0x1002);

    EXPECT_EQ(session_mgr_->get_active_session_count(), 2);

    session_mgr_->cleanup_expired_sessions(std::chrono::seconds(0));
    EXPECT_EQ(session_mgr_->get_active_session_count(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
