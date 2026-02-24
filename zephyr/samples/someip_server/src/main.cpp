/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * SOME/IP server running on Zephyr native_sim.
 *
 * Listens on 127.0.0.1:30490 for SOME/IP REQUEST messages, echoes
 * the payload back inside a RESPONSE with a "Server got: " prefix.
 * Demonstrates real UDP networking between two Zephyr processes.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>

#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "transport/endpoint.h"
#include "transport/udp_transport.h"
#include "platform/thread.h"

using namespace someip;
using namespace someip::transport;

static constexpr uint16_t SERVICE_ID    = 0x1000;
static constexpr uint16_t METHOD_HELLO  = 0x0001;
static constexpr uint16_t LISTEN_PORT   = 30490;

class Server : public ITransportListener {
public:
    Server() : transport_(Endpoint("0.0.0.0", LISTEN_PORT)) {
        transport_.set_listener(this);
    }

    bool start() {
        if (transport_.start() != Result::SUCCESS) {
            printf("[server] Failed to start transport\n");
            return false;
        }
        auto ep = transport_.get_local_endpoint();
        printf("[server] Listening on %s:%d\n",
               ep.get_address().c_str(), ep.get_port());
        return true;
    }

    void stop() { transport_.stop(); }

    uint32_t messages_handled() const { return count_.load(); }

    void on_message_received(MessagePtr msg, const Endpoint& sender) override {
        printf("[server] Message from %s:%d  service=0x%04X method=0x%04X\n",
               sender.get_address().c_str(), sender.get_port(),
               msg->get_service_id(), msg->get_method_id());

        if (msg->get_service_id() != SERVICE_ID ||
            msg->get_method_id() != METHOD_HELLO ||
            !msg->is_request()) {
            printf("[server] Ignoring non-matching message\n");
            return;
        }

        std::string text(msg->get_payload().begin(), msg->get_payload().end());
        printf("[server] Client says: '%s'\n", text.c_str());

        std::string reply = "Server got: " + text;
        Message response(
            MessageId(SERVICE_ID, METHOD_HELLO),
            RequestId(msg->get_client_id(), msg->get_session_id()),
            MessageType::RESPONSE,
            ReturnCode::E_OK);
        response.set_payload(std::vector<uint8_t>(reply.begin(), reply.end()));

        auto rc = transport_.send_message(response, sender);
        if (rc == Result::SUCCESS) {
            printf("[server] Replied: '%s'\n", reply.c_str());
            count_.fetch_add(1);
        } else {
            printf("[server] Send failed: %d\n", static_cast<int>(rc));
        }
    }

    void on_connection_lost(const Endpoint&) override {}
    void on_connection_established(const Endpoint&) override {}
    void on_error(Result err) override {
        printf("[server] Transport error: %d\n", static_cast<int>(err));
    }

private:
    UdpTransport transport_;
    std::atomic<uint32_t> count_{0};
};

int main() {
    printf("=== SOME/IP Server (Zephyr native_sim) ===\n");

    Server server;
    if (!server.start()) {
        return 1;
    }

    printf("[server] Waiting for requests... (runs until killed)\n");

    while (true) {
        platform::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
