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

#ifndef SOMEIP_TRANSPORT_TCP_TRANSPORT_H
#define SOMEIP_TRANSPORT_TCP_TRANSPORT_H

#include "transport/transport.h"
#include "platform/buffer_pool.h"
#include "platform/containers.h"
#include "platform/net.h"
#include "platform/thread.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

/**
 * @brief Compile-time upper bound on concurrently served TCP connections.
 *
 * Static builds size the connection table at compile time, so this bound also
 * caps TcpTransportConfig::max_connections. Each served connection may hold a
 * receive buffer drawn from the byte pool, so raising this on a static build
 * usually means raising SOMEIP_BYTE_POOL_* counts as well.
 */
#ifndef SOMEIP_MAX_TCP_CONNECTIONS            // NOLINT(cppcoreguidelines-macro-usage)
#define SOMEIP_MAX_TCP_CONNECTIONS 10         // NOLINT(cppcoreguidelines-macro-usage)
#endif

namespace someip::transport {

inline constexpr size_t MAX_TCP_CONNECTIONS = SOMEIP_MAX_TCP_CONNECTIONS;

/**
 * @brief TCP Connection State
 */
enum class TcpConnectionState : uint8_t {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

/**
 * @brief TCP Connection Information
 */
struct TcpConnection {
    someip_socket_t socket_fd{SOMEIP_INVALID_SOCKET};
    Endpoint remote_endpoint;
    TcpConnectionState state{TcpConnectionState::DISCONNECTED};
    std::chrono::steady_clock::time_point last_activity{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point last_magic_cookie{std::chrono::steady_clock::now()};
    platform::ByteBuffer receive_buffer;

    TcpConnection() = default;

    bool is_connected() const {
        return state == TcpConnectionState::CONNECTED;
    }

    void update_activity() {
        last_activity = std::chrono::steady_clock::now();
    }
};

/**
 * @brief TCP Transport Configuration
 */
struct TcpTransportConfig {
    std::chrono::milliseconds connection_timeout{5000};     // Connection timeout
    std::chrono::milliseconds receive_timeout{100};        // Receive timeout
    std::chrono::milliseconds send_timeout{1000};          // Send timeout
    size_t max_receive_buffer{65536};                       // Max receive buffer size
    size_t max_connections{10};                             // Concurrent connections (clamped)
    bool keep_alive{true};                                  // TCP keep-alive
    std::chrono::milliseconds keep_alive_interval{30000};   // Keep-alive interval
    bool magic_cookie_enabled{true};                        // Periodic Magic Cookie insertion
    std::chrono::milliseconds magic_cookie_interval{10000}; // 10s per SOME/IP spec
};

/**
 * @brief TCP Transport Implementation
 *
 * Provides reliable, connection-oriented transport for SOME/IP messages
 * using TCP sockets. Supports both client and server modes.
 */
class TcpTransport : public ITransport {
public:
    /**
     * @brief Constructor
     * @param config TCP transport configuration
     */
    explicit TcpTransport(const TcpTransportConfig& config = TcpTransportConfig());

    /**
     * @brief Destructor
     */
    ~TcpTransport() override;

    // Delete copy and move operations
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;
    TcpTransport(TcpTransport&&) = delete;
    TcpTransport& operator=(TcpTransport&&) = delete;

    /**
     * @brief Initialize the transport
     * @param local_endpoint Local endpoint to bind to
     * @return Result of the operation
     */
    [[nodiscard]] Result initialize(const Endpoint& local_endpoint);

    /**
     * @brief Send a message to one connected peer
     *
     * The message is sent on the connection whose remote endpoint matches
     * @p endpoint by address and port. A server therefore addresses each of its
     * peers individually; a client names the endpoint it connected to.
     *
     * @param message The message to send
     * @param endpoint The destination endpoint
     * @return SUCCESS on transmission, INVALID_ENDPOINT if @p endpoint is
     *         malformed, NOT_CONNECTED if no connection matches it, TIMEOUT if
     *         the peer accepted nothing within TcpTransportConfig::send_timeout
     *         (the message did not go out and may be retried), CONNECTION_LOST
     *         if that deadline passed with the message only partly written, in
     *         which case the peer has been closed because its stream can no
     *         longer be framed, NETWORK_ERROR on a socket failure
     *
     * @thread_safety Thread-safe
     * @safety Safety alignment in progress (not certified)
     */
    [[nodiscard]] Result send_message(const Message& message, const Endpoint& endpoint) override;

    /**
     * @brief Receive a message from the internal queue (non-blocking, polling mode)
     *
     * Only returns messages when no listener is installed via set_listener().
     * Messages queued before listener installation remain drainable.
     *
     * @return Received message or nullptr if no message available
     * @see set_listener()
     */
    MessagePtr receive_message() override;

    /**
     * @brief Connect to a remote endpoint
     * @param endpoint The endpoint to connect to
     * @return Result of the operation
     */
    Result connect(const Endpoint& endpoint) override;

    /**
     * @brief Close every connection this transport holds
     *
     * In server mode that is all accepted peers, not a single connection; use
     * disconnect_peer() to close one peer and leave the rest serving.
     * ITransportListener::on_connection_lost() is reported for each peer closed.
     *
     * @return SUCCESS, including when nothing was connected
     */
    Result disconnect() override;

    /**
     * @brief Check if connected to remote endpoint
     * @return true if connected, false otherwise
     */
    bool is_connected() const override;

    /**
     * @brief Get local endpoint
     * @return Local endpoint information
     */
    Endpoint get_local_endpoint() const override;

    /**
     * @brief Set transport listener for asynchronous message delivery
     *
     * @copydetails ITransport::set_listener()
     */
    void set_listener(ITransportListener* listener) override;

    /**
     * @brief Start the transport
     * @return Result of the operation
     */
    Result start() override;

    /**
     * @brief Stop the transport
     * @return Result of the operation
     */
    Result stop() override;

    /**
     * @brief Check if transport is running
     * @return true if running, false otherwise
     */
    bool is_running() const override;

    /**
     * @brief Get current connection state
     *
     * A server may serve several peers at once, so this collapses the whole
     * table to one value: CONNECTED if any peer is connected, otherwise
     * DISCONNECTING if any peer is being torn down, otherwise DISCONNECTED.
     * CONNECTING is never reported here, because an outbound connection occupies
     * no slot until it completes and connect() blocks for its duration. Use
     * connection_count() or is_peer_connected() to inspect individual peers.
     *
     * @return Aggregate connection state
     */
    TcpConnectionState get_connection_state() const;

    /**
     * @brief Number of peers currently connected
     *
     * @return Connection count (0..max_connections())
     *
     * @thread_safety Thread-safe
     * @safety Safety alignment in progress (not certified)
     */
    size_t connection_count() const;

    /**
     * @brief Effective concurrent connection limit
     *
     * TcpTransportConfig::max_connections clamped to SOMEIP_MAX_TCP_CONNECTIONS.
     *
     * @return Maximum number of peers served concurrently
     *
     * @thread_safety Thread-safe
     * @safety Safety alignment in progress (not certified)
     */
    size_t max_connections() const;

    /**
     * @brief Check whether a specific peer is connected
     *
     * @param peer Remote endpoint, matched on address and port
     * @return true if a connection to that peer is established
     *
     * @thread_safety Thread-safe
     * @safety Safety alignment in progress (not certified)
     */
    bool is_peer_connected(const Endpoint& peer) const;

    /**
     * @brief Close the connection to one peer, leaving others untouched
     *
     * Reports ITransportListener::on_connection_lost() for the closed peer.
     *
     * @param peer Remote endpoint, matched on address and port
     * @return SUCCESS if the peer was connected and is now closed,
     *         NOT_CONNECTED otherwise
     *
     * @thread_safety Thread-safe. Safe to call from a listener callback.
     * @safety Safety alignment in progress (not certified)
     */
    Result disconnect_peer(const Endpoint& peer);

    /**
     * @brief Enable server mode (listen for incoming connections)
     * @param backlog Maximum number of pending connections
     * @return Result of the operation
     */
    Result enable_server_mode(int backlog = 5);

    /**
     * @brief Accept incoming connection (server mode)
     * @return New connection socket FD or -1 on error
     */
    someip_socket_t accept_connection();

    /**
     * @brief Parse one complete SOME/IP message from a byte buffer.
     *
     * Consumes exactly the bytes of one message if successful; leaves
     * incomplete trailing bytes in the buffer for subsequent calls.
     *
     * @param buffer Accumulation buffer (modified in-place)
     * @param message [out] Parsed message on success
     * @return true if a complete message was extracted
     */
    bool parse_message_from_buffer(platform::ByteBuffer& buffer, MessagePtr& message);

    static constexpr size_t SOMEIP_HEADER_SIZE = 16;
    static constexpr size_t MAX_MESSAGE_SIZE = 65535;

    /** @implements REQ_TRANSPORT_020, REQ_TRANSPORT_025 */
    static bool is_magic_cookie(const platform::ByteBuffer& data, size_t offset = 0);
    static platform::ByteBuffer make_magic_cookie_client();
    static platform::ByteBuffer make_magic_cookie_server();

private:
    /// Peer endpoints batched for notification outside table_mutex_.
    using EndpointList = platform::Vector<Endpoint, MAX_TCP_CONNECTIONS>;

    enum class SlotState : uint8_t {
        FREE,      ///< Unused and available for a new peer.
        ACTIVE,    ///< Serving a peer.
        CLOSING    ///< Being torn down; no new I/O may start on it.
    };

    /**
     * @brief One peer connection and the lock serialising its socket I/O.
     *
     * Slots live in a fixed array and are never moved or erased, only recycled
     * through FREE. That matters for two reasons: a slot pointer stays valid
     * once table_mutex_ is released, and the slot can own a platform::Mutex,
     * which a vector could not hold because erase() moves its elements and a
     * mutex is not movable.
     */
    struct ConnectionSlot {
        TcpConnection conn;
        /// Held across this peer's blocking socket calls. Never acquired while
        /// table_mutex_ is held; see the lock-order note on table_mutex_.
        platform::Mutex io_mutex;
        SlotState state{SlotState::FREE};
        /// Bumped every time the slot returns to FREE, so it names one session
        /// rather than one slot. A thread that captured the slot while it was
        /// serving peer A and then had to release table_mutex_ compares this on
        /// its way back in; a mismatch means the slot was recycled meanwhile,
        /// which neither the peer endpoint nor the descriptor number can reveal
        /// on their own since both may be reissued.
        uint32_t generation{0};
    };

    TcpTransportConfig config_;
    Endpoint local_endpoint_;
    std::atomic<ITransportListener*> listener_{nullptr};

    std::atomic<bool> running_{false};
    std::optional<platform::Thread> receive_thread_;
    std::optional<platform::Thread> connection_thread_;

    platform::Queue<std::pair<MessagePtr, Endpoint>> message_queue_;
    platform::Mutex queue_mutex_;
    platform::ConditionVariable queue_cv_;

    /**
     * @brief Guards the slot table, server_mode_ and both socket descriptors.
     *
     * No blocking socket call is ever made while this is held, so one slow or
     * unresponsive peer cannot stall accepts or traffic for the others. Peer
     * I/O is serialised by ConnectionSlot::io_mutex instead.
     *
     * Lock order is io_mutex then table_mutex_, never the reverse: a thread
     * releases table_mutex_ before waiting on a slot's io_mutex, then re-takes
     * table_mutex_ to re-validate the slot. Listener callbacks are invoked with
     * both released, so a listener may re-enter the transport.
     */
    mutable platform::Mutex table_mutex_;
    std::array<ConnectionSlot, MAX_TCP_CONNECTIONS> slots_;
    bool server_mode_{false};

    /// Socket created by initialize(). Ownership moves to listen_socket_fd_ on
    /// enable_server_mode(), or into a slot on a successful connect().
    someip_socket_t bound_socket_fd_{SOMEIP_INVALID_SOCKET};
    someip_socket_t listen_socket_fd_{SOMEIP_INVALID_SOCKET};

    void deliver_or_enqueue(const MessagePtr& message, const Endpoint& sender);
    someip_socket_t accept_connection_with_peer(Endpoint& peer_endpoint);
    Result create_socket();
    Result bind_socket();
    Result setup_socket_options(someip_socket_t socket_fd, bool blocking = true);
    Result connect_internal(const Endpoint& endpoint);
    void disconnect_internal();
    void receive_loop();
    void connection_monitor_loop();
    void send_periodic_magic_cookie();
    Result send_data(someip_socket_t socket_fd, const platform::ByteBuffer& data);

    /**
     * @brief Read once from a socket into a caller-owned buffer.
     *
     * Takes no lock, so the caller may hold a slot's io_mutex across it without
     * blocking the slot table. Reads at most @p max_chunk bytes, which the
     * caller derives from how much room is left in the connection's buffer.
     * @p received is set to the byte count on success.
     */
    Result receive_data(someip_socket_t socket_fd, std::array<uint8_t, 4096>& buffer,
                        size_t max_chunk, size_t& received);

    // Slot table helpers. The _locked suffix requires table_mutex_.
    ConnectionSlot* find_active_peer_locked(const Endpoint& peer);
    ConnectionSlot* find_active_socket_locked(someip_socket_t socket_fd);
    ConnectionSlot* allocate_slot_locked();
    size_t active_count_locked() const;

    /**
     * @brief Take exclusive I/O rights on a peer's slot.
     *
     * Waits on the slot's io_mutex with table_mutex_ released, then re-validates
     * that the slot still holds the same session, since it may have been closed
     * and recycled meanwhile. On success the slot's io_mutex is held by the
     * caller and must be released with release_io(); @p fd_out receives the
     * descriptor, which cannot be closed while those rights are held.
     *
     * @return The slot, or nullptr if that session is over.
     */
    ConnectionSlot* acquire_io(const Endpoint& peer, someip_socket_t& fd_out);
    /// As above, identifying the slot by descriptor rather than by peer.
    ConnectionSlot* acquire_io(someip_socket_t socket_fd);
    static void release_io(ConnectionSlot& slot);

    /**
     * @brief Reserve an ACTIVE slot for teardown. Requires table_mutex_.
     *
     * Moving ACTIVE -> CLOSING inside the same critical section that found the
     * slot is what makes teardown safe to finish with the table released:
     * allocate_slot_locked() only takes FREE slots and the find_active_*_locked()
     * helpers skip CLOSING ones, so no other thread can hand the slot to a new
     * peer or claim it for a second close. The claiming thread is therefore the
     * only one that will close this descriptor and the only one that reports the
     * loss. Pair every claim with finish_close().
     */
    static void claim_close_locked(ConnectionSlot& slot);

    /// Complete a teardown reserved by claim_close_locked(), returning the slot
    /// to FREE under a new generation. Must NOT hold table_mutex_.
    void finish_close(ConnectionSlot& slot);

    /// Claim, close and report one peer. @return false if it was not connected.
    bool close_peer_and_notify(const Endpoint& peer);
    void close_socket_and_notify(someip_socket_t socket_fd);
    void notify_peer_lost(const Endpoint& peer);
    void notify_peers_lost(const EndpointList& peers);

    /// Accept one pending peer (server mode), if below the connection limit.
    void accept_pending_peer();
    /// Read from one peer socket and dispatch every complete message on it.
    void service_peer(someip_socket_t socket_fd);
};

}  // namespace someip::transport

#endif // SOMEIP_TRANSPORT_TCP_TRANSPORT_H
