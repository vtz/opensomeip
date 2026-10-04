..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
Transport Requirements
==============================

This section defines requirements for the transport layer implementation
in OpenSOMEIP, including UDP and TCP transports.

Overview
========

The transport layer provides:

1. UDP transport with multicast support
2. TCP transport with connection management
3. Abstract transport interface for extensibility

Requirements
============

UDP Transport
-------------

.. requirement:: UDP Bind and Unicast Send/Receive
   :id: REQ_TRANSPORT_001a
   :satisfies: feat_req_someip_32, feat_req_someip_318, feat_req_someip_319, feat_req_someip_33
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Bind UdpTransport to 0.0.0.0:30490, send unicast message to 127.0.0.1:30491, verify reception. Verify bind rejects invalid port.

   The implementation shall provide UDP transport that supports binding
   to local address and port, sending messages to unicast destinations,
   and receiving messages from any source.

   **Rationale**: UDP unicast is the foundation for SOME/IP point-to-point
   communication.

   **Code Location**: ``src/transport/udp_transport.cpp``,
   ``src/transport/event_driven_udp_transport.cpp``

.. requirement:: UDP Multicast Support
   :id: REQ_TRANSPORT_001b
   :satisfies: feat_req_someip_584, feat_req_someip_811, feat_req_someip_315, feat_req_someip_316
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Join multicast group 239.0.0.1, send multicast message, verify reception on joined socket. Verify leave_multicast_group stops reception.

   The implementation shall support sending to multicast destinations and
   multicast group join/leave for receiving multicast traffic.

   **Rationale**: Multicast is required for Service Discovery and
   eventgroup subscriptions.

   **Code Location**: ``src/transport/udp_transport.cpp``,
   ``src/transport/event_driven_udp_transport.cpp``,
   ``include/transport/multicast_transport.h``

.. requirement:: Non-Blocking I/O and Thread Safety
   :id: REQ_TRANSPORT_001c
   :satisfies: feat_req_someip_659, feat_req_someip_664, feat_req_someip_317
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Call send/receive from 2 threads concurrently, verify no data corruption. Verify poll/select used with configurable timeout for non-blocking behavior.

   The implementation shall provide non-blocking I/O with configurable
   timeouts and thread-safe operation for concurrent send/receive.

   **Rationale**: Non-blocking I/O and thread safety enable integration
   with event loops and multi-threaded applications.

   **Code Location**: ``src/transport/udp_transport.cpp``

TCP Transport
-------------

.. requirement:: TCP Client/Server Modes
   :id: REQ_TRANSPORT_002a
   :satisfies: feat_req_someip_32, feat_req_someip_325, feat_req_someip_323, feat_req_someip_324
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Create TcpTransport in client mode, connect to server, verify handshake. Create in server mode, accept connection, verify data exchange.

   The implementation shall provide a TCP transport (``TcpTransport``)
   that supports client mode (connect to remote server) and server mode
   (accept incoming connections).

   **Rationale**: Client and server modes cover both initiator and
   responder roles in TCP communication.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: TCP Framing and State Management
   :id: REQ_TRANSPORT_002b
   :satisfies: feat_req_someip_585, feat_req_someip_644, feat_req_someip_645, feat_req_someip_661
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Send 3 SOME/IP messages over TCP, verify each is framed correctly and received as separate messages. Verify connection state transitions (connecting/connected/disconnected).

   The implementation shall provide message framing over TCP streams,
   connection state management, automatic reconnection (configurable),
   and thread-safe operation.

   **Rationale**: TCP streams require framing to delimit messages;
   state management enables robust reconnection.

   **Code Location**: ``src/transport/tcp_transport.cpp``

Connection Management
---------------------

.. requirement:: Connection State Tracking and Notification
   :id: REQ_TRANSPORT_003a
   :satisfies: feat_req_someip_326, feat_req_someip_644, feat_req_someip_646
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Connect TCP client, verify state transitions disconnected->connecting->connected. Register listener, verify on_connected/on_disconnected callbacks invoked.

   The TCP transport shall track connection state (disconnected, connecting,
   connected) and notify listeners of connection state changes.

   **Rationale**: State tracking and notifications enable applications
   to react to connection lifecycle events.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: Graceful Shutdown and Error Handling
   :id: REQ_TRANSPORT_003b
   :satisfies: feat_req_someip_647, feat_req_someip_678, feat_req_someip_679, feat_req_someip_680
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Call disconnect(), verify TCP FIN sent and socket closed cleanly. Simulate connection error, verify state transitions to disconnected and pending requests timeout. Connect several clients concurrently, verify each stays connected, that a response reaches only its addressee, and that disconnect_peer() closes one peer while the others keep serving.

   The TCP transport shall support graceful connection shutdown, handle
   connection errors and timeouts, and support multiple simultaneous
   connections (server mode).

   **Rationale**: Graceful shutdown and error handling ensure clean
   resource release and predictable behavior on failures.

   **Code Location**: ``src/transport/tcp_transport.cpp``

Error Recovery
--------------

.. requirement:: Retry Send on Transient Errors
   :id: REQ_TRANSPORT_004a
   :satisfies: feat_req_someip_429, feat_req_someip_430, feat_req_someip_434, feat_req_someip_435
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Simulate transient send failure (EAGAIN), verify transport retries send up to configured limit and succeeds on retry.

   The transport layer shall retry send operations on transient errors
   (e.g., EAGAIN, EWOULDBLOCK) according to configurable retry policy.

   **Rationale**: Transient errors are common in non-blocking I/O and often
   resolve on retry.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``src/transport/tcp_transport.cpp``

.. requirement:: Socket Close/Reopen on Persistent Errors
   :id: REQ_TRANSPORT_004b
   :satisfies: feat_req_someip_436, feat_req_someip_437, feat_req_someip_438
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Simulate persistent send failure (EPIPE, ECONNRESET), verify socket is closed and reopened before next send attempt.

   The transport layer shall close and reopen sockets when persistent
   errors occur (e.g., connection reset, broken pipe).

   **Rationale**: Persistent errors indicate the socket is unusable;
   reopening restores communication.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``src/transport/tcp_transport.cpp``

.. requirement:: Error Logging with Detail
   :id: REQ_TRANSPORT_004c
   :satisfies: feat_req_someip_439, feat_req_someip_440, feat_req_someip_441
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Trigger send failure, verify log contains errno, endpoint address, and operation context (send/receive).

   The transport layer shall log errors with sufficient detail for
   diagnosis (errno, endpoint, operation context).

   **Rationale**: Detailed error logs enable rapid troubleshooting in
   production environments.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``src/transport/tcp_transport.cpp``

.. requirement:: Return Error Codes and Configurable Retry
   :id: REQ_TRANSPORT_004d
   :satisfies: feat_req_someip_326, feat_req_someip_371, feat_req_someip_442, feat_req_someip_443
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Configure retry_count=3 and retry_delay_ms=100, verify caller receives error code after retries exhausted. Verify config changes affect retry behavior.

   The transport layer shall return appropriate error codes to callers
   and support configurable retry policies (count, delay).

   **Rationale**: Error codes allow application-layer handling; configurable
   retry adapts to different deployment requirements.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``src/transport/tcp_transport.cpp``

Transport Interface
-------------------

.. requirement:: Abstract Transport Interface
   :id: REQ_TRANSPORT_005
   :satisfies: feat_req_someip_32, feat_req_someip_56, feat_req_someip_31
   :status: implemented
   :priority: high
   :verification: Code inspection of interface definition and successful compilation of transport-agnostic application code.

   The implementation shall provide an abstract transport interface
   (``ITransport``) that defines:

   * ``start()``: Initialize and start the transport
   * ``stop()``: Stop the transport and release resources
   * ``send(Message&, Endpoint&)``: Send a message
   * ``set_listener(ITransportListener*)``: Set message listener
   * ``is_running()``: Check transport state

   **Rationale**: Abstract interface enables transport-agnostic
   application code and testing.

   **Code Location**: ``include/transport/transport.h``,
   ``src/transport/event_driven_udp_transport.cpp``,
   ``src/transport/event_driven_tcp_transport.cpp``

Endpoint Configuration
----------------------

.. requirement:: Endpoint Configuration
   :id: REQ_TRANSPORT_006
   :satisfies: feat_req_someip_32, feat_req_someip_659, feat_req_someip_660, feat_req_someip_661, feat_req_someip_733
   :status: implemented
   :priority: medium
   :verification: Code inspection of endpoint validation logic and successful parsing of valid/invalid endpoint configurations.

   The implementation shall provide endpoint configuration:

   * IP address (IPv4 and IPv6)
   * Port number
   * Protocol type (UDP/TCP)
   * Validation of endpoint parameters
   * String representation for logging

   **Rationale**: Proper endpoint handling is fundamental to
   network communication.

   **Code Location**: ``include/transport/endpoint.h``

Transport Protocol Binding
==========================

.. requirement:: nPDU Feature Support
   :id: REQ_TRANSPORT_010
   :satisfies: feat_req_someip_702, feat_req_someip_741, feat_req_someip_663
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Integration test: Send 3 SOME/IP messages in a single UDP datagram, receive and verify all 3 are parsed correctly. TCP: see REQ_TRANSPORT_027.

   The software shall support transporting more than one SOME/IP message
   in a single transport layer PDU (nPDU feature). For cyclic senders,
   nPDU shall be supported without explicit configuration. Receive-side
   TCP multi-message extraction and ordered delivery are refined in
   ``REQ_TRANSPORT_027``.

   **Rationale**: nPDU reduces per-message overhead for high-frequency communication.

   **Code Location**: ``src/transport/udp_transport.cpp`` (send_data, receive_loop),
   ``src/transport/event_driven_tcp_transport.cpp``, ``src/transport/tcp_transport.cpp``

.. requirement:: UDP Multicast Support
   :id: REQ_TRANSPORT_011
   :satisfies: feat_req_someip_811, feat_req_someip_812, feat_req_someip_814
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Join multicast group 239.0.0.1, send multicast message, verify reception. Verify unicast initial field event after subscription.

   The software shall support UDP unicast and multicast transmission.
   Multicast eventgroups shall use unicast for initial field events.
   Clients shall support receiving via unicast and/or multicast.

   **Rationale**: Multicast reduces bandwidth for events delivered to many subscribers.

   **Code Location**: ``src/transport/udp_transport.cpp`` (join_multicast_group, configure_multicast),
   ``src/transport/event_driven_udp_transport.cpp``, ``include/transport/multicast_transport.h``

.. requirement:: Multicast Threshold Switching
   :id: REQ_TRANSPORT_012
   :satisfies: feat_req_someip_813
   :status: draft
   :priority: medium
   :category: happy_path
   :verification: Unit test: Configure multicast threshold=3, subscribe 2 clients (unicast), subscribe 3rd (switch to multicast), verify mode change.

   The software shall support dynamic switching of eventgroups between
   unicast and multicast based on the Multicast-Threshold configuration.

   **Rationale**: Dynamic switching optimizes bandwidth based on actual subscriber count.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``include/sd/sd_types.h``

.. requirement:: Internal Message Multiplexing
   :id: REQ_TRANSPORT_013
   :satisfies: feat_req_someip_732
   :status: draft
   :priority: high
   :category: happy_path
   :verification: Unit test: Send messages for Service A and Service B on same UDP socket, verify each is dispatched to the correct handler by Service ID.

   The software shall support internal de/multiplexing of SOME/IP messages
   independent of the number of controllers, cores, and IP addresses.

   **Rationale**: Internal multiplexing decouples service routing from physical network topology.

   **Code Location**: ``src/rpc/rpc_server.cpp`` (service_id_ filtering), ``src/sd/sd_server.cpp``

.. requirement:: Port Configuration
   :id: REQ_TRANSPORT_014
   :satisfies: feat_req_someip_658, feat_req_someip_676, feat_req_someip_733
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Initialize EventSubscriber with no resolver and no default endpoint. subscribe_eventgroup and request_field fail before send and deliver no datagram to port 30490. With an explicit non-SD destination, the same calls send the application message to that destination. SOMEIP_SD_MULTICAST_ENDPOINT stays on 30490. Generic application defaults are unresolved (empty address, port 0), not a fixed application port. Ephemeral local bind with an explicit address and port 0 still succeeds.

   The software shall read port numbers from configuration files.
   Port 30490 shall only be used for SOME/IP-SD, not application
   communication.

   **Rationale**: Configuration-driven ports ensure deterministic network resource allocation.

   **Code Location**: ``src/events/event_subscriber.cpp`` (unresolved remote destination fails before send), ``src/transport/endpoint.cpp`` (application defaults are unresolved; SD multicast stays on 30490), ``include/transport/udp_transport.h`` (UdpTransportConfig), ``include/sd/sd_types.h`` (SdConfig)

.. requirement:: Ephemeral Port Range
   :id: REQ_TRANSPORT_015
   :satisfies: feat_req_someip_661
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Request dynamic port and verify allocated port is in range 49152-65535.

   The software shall use IETF/IANA ephemeral port range (49152-65535)
   for dynamic ports.

   **Rationale**: Ephemeral port range compliance avoids conflicts with well-known services.

   **Code Location**: ``include/transport/endpoint.h``, ``src/transport/udp_transport.cpp``


TCP Connection Management
=========================

.. requirement:: Client-Initiated TCP Connection
   :id: REQ_TRANSPORT_016
   :satisfies: feat_req_someip_646, feat_req_someip_647
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Create TcpTransport in client mode, call connect_internal(), verify TCP handshake completes. Simulate disconnect, verify reconnection attempt.

   The software shall open TCP connections from the client side when
   the first method call or notification subscription is needed.
   The client is responsible for re-establishing failed connections.

   **Rationale**: Client-initiated connections simplify server-side connection management.

   **Code Location**: ``src/transport/tcp_transport.cpp`` (connect_internal)

.. requirement:: TCP Connection Sharing
   :id: REQ_TRANSPORT_017
   :satisfies: feat_req_someip_644, feat_req_someip_645
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Register 2 services on same remote endpoint, verify single TCP connection is created. Send requests for both services over same connection.

   The software shall use a single TCP connection for all methods, events,
   and notifications of a service instance. TCP connections shall be
   shared across services to minimize connection count.

   **Rationale**: Connection sharing minimizes TCP connection overhead and OS resource usage.

   **Code Location**: ``src/transport/tcp_transport.cpp`` (connection sharing logic)

.. requirement:: TCP Connection Closure
   :id: REQ_TRANSPORT_018
   :satisfies: feat_req_someip_678, feat_req_someip_679, feat_req_someip_680
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Disconnect TCP client, verify TCP FIN is sent. Verify server does not initiate disconnection preemptively.

   TCP connections shall be closed by the client when no longer required
   or when all services are unavailable. The server shall not close
   connections preemptively.

   **Rationale**: Client-controlled closure prevents servers from disrupting active communication.

   **Code Location**: ``src/transport/tcp_transport.cpp`` (disconnect, disconnect_internal)

.. requirement:: TCP Timeout on Connection Loss
   :id: REQ_TRANSPORT_019
   :satisfies: feat_req_someip_326, feat_req_someip_681
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Simulate TCP connection loss with pending request, verify request callback receives TIMEOUT error within configured timeout.

   The software shall treat outstanding requests as timeouts when the
   TCP connection is lost.

   **Rationale**: Timeout handling prevents clients from waiting indefinitely for responses on broken connections.

   **Code Location**: ``src/transport/tcp_transport.cpp`` (connection_monitor_loop)


Magic Cookie Support
====================

.. requirement:: TCP Magic Cookie Messages
   :id: REQ_TRANSPORT_020
   :satisfies: feat_req_someip_586, feat_req_someip_591, feat_req_someip_592, feat_req_someip_609, feat_req_someip_619
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Construct Magic Cookie message (SID=0xFFFF, MID=0x0000, Len=8, CID=0xDEAD, SessID=0xBEEF), inject into TCP stream, verify stream resync.

   The software shall support SOME/IP Magic Cookie Messages for TCP
   stream resynchronization. Each TCP segment shall start with a Magic
   Cookie (Service ID 0xFFFF, Method ID 0x0000/0x8000, Length 8,
   Client ID 0xDEAD, Session ID 0xBEEF).

   **Rationale**: Magic Cookies enable TCP stream resynchronization after framing errors.

   **Code Location**: ``src/transport/tcp_transport.cpp`` (parse_message_from_buffer)

.. requirement:: Magic Cookie Fallback Heuristic
   :id: REQ_TRANSPORT_021
   :satisfies: feat_req_someip_593, feat_req_someip_594
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Simulate active TCP connection without Magic Cookie access, verify Magic Cookie is inserted after 10 seconds of activity.

   If the software cannot access TCP segmentation, it shall insert
   Magic Cookie Messages every 10 seconds on active connections.

   **Rationale**: Periodic Magic Cookies provide a fallback when TCP segmentation info is unavailable.

   **Code Location**: ``src/transport/tcp_transport.cpp``


Service Instance Binding
========================

.. requirement:: Multiple Service Instance Port Binding
   :id: REQ_TRANSPORT_022
   :satisfies: feat_req_someip_445, feat_req_someip_636, feat_req_someip_967, feat_req_someip_1079, feat_req_someip_444
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Register 3 instances of same service on ports 30001, 30002, 30003, verify each accepts connections independently.

   The software shall support multiple service instances on different
   ECUs and on the same ECU. Multiple instances of the same service
   on one ECU shall listen on different ports.

   **Rationale**: Multi-port binding enables horizontal scaling of service instances.

   **Code Location**: ``src/sd/sd_server.cpp`` (offer_service per instance), ``include/sd/sd_types.h``

.. requirement:: Client Server Address Resolution
   :id: REQ_TRANSPORT_023
   :satisfies: feat_req_someip_660, feat_req_someip_665
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Receive OfferService with IPv4EndpointOption containing IP=10.0.0.1:30001, verify client connects to that address.

   The client shall use the IP address and port number announced by the
   server via SOME/IP-SD for communication.

   **Rationale**: SD-announced address resolution ensures clients connect to the correct server endpoint.

   **Code Location**: ``src/sd/sd_client.cpp`` (process_offer_entry, ip_address from option)

.. requirement:: Unaligned Message Reception
   :id: REQ_TRANSPORT_024
   :satisfies: feat_req_someip_664
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Send 2 SOME/IP messages back-to-back in single UDP PDU with no alignment, verify both are parsed correctly.

   The software shall be capable of receiving unaligned SOME/IP messages
   when multiple messages are transported in a single UDP or TCP PDU.

   **Rationale**: Unaligned message handling supports nPDU and multi-message PDUs.
   ``feat_req_someip_668`` is an informational cross-reference about Request ID
   handling in SOME/IP-SD (deferred to the SD chapter), not unaligned reception;
   it is intentionally left unmapped rather than attached here.

   **Code Location**: ``src/transport/tcp_transport.cpp`` (parse_message_from_buffer), ``src/transport/udp_transport.cpp``, ``src/transport/event_driven_tcp_transport.cpp``

.. requirement:: TCP Stream Multi-Message Extraction and Ordered Delivery
   :id: REQ_TRANSPORT_027
   :satisfies: feat_req_someip_702, feat_req_someip_664, feat_req_someip_585, feat_req_someip_77
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Inject one TCP adapter callback containing N concatenated complete SOME/IP frames plus an incomplete trailing fragment; verify all N frames are delivered in order to the listener (or polling queue when no listener), that delivery of each frame completes before the next frame is parsed, that no bounded MessagePtr staging vector is used, and that only the incomplete trailing bytes remain in the reassembly buffer. Covered by ConcatenatedBurstDeliversAllInOrder, ConcatenatedBurstEnqueuedWithoutListener, IncompleteTrailingFrameLeftForNextCallback.

   When a TCP segment carries one or more SOME/IP messages (nPDU on TCP),
   the transport shall extract every complete message in stream order.
   Message boundaries shall be determined from the SOME/IP Length field.
   Each complete message shall be delivered to the installed
   ``ITransportListener`` **or** enqueued for ``receive_message()``
   (exclusive; never both) before the next complete message is parsed from
   the reassembly buffer. The transport shall not accumulate complete
   frames into a bounded ``MessagePtr`` staging container before delivery.
   Only an incomplete trailing fragment may remain buffered after processing
   a receive callback.

   **Rationale**: feat_req_someip_702 requires nPDU receive on TCP segments;
   feat_req_someip_77 defines Length-based framing; feat_req_someip_585
   requires a SOME/IP header per payload; feat_req_someip_664 requires
   unaligned multi-message reception. One-at-a-time delivery avoids silent
   truncation on capacity-bounded containers.

   **Code Location**: ``src/transport/event_driven_tcp_transport.cpp``
   (``on_adapter_receive``, ``parse_next_message``, ``deliver_or_enqueue``),
   ``src/transport/tcp_transport.cpp`` (``receive_loop``, ``parse_next_message``)

.. requirement:: TCP Reassembly Buffer Retention and Exhaustion
   :id: REQ_TRANSPORT_028
   :satisfies: feat_req_someip_702, feat_req_someip_435
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: After delivering any complete frames from a callback, force the retained incomplete reassembly bytes (or a declared frame) past ``max_receive_buffer``; verify the reassembly buffer is discarded, ``Result::BUFFER_OVERFLOW`` is signaled via ``on_message_rejected`` (TCP framing stage), no complete unparsed PDUs remain buffered, and the drop is not silent. Covered by ReceiveBufferExhaustionResetsAndSignals.

   After extracting complete messages, only an incomplete trailing fragment
   shall be retained in the TCP reassembly buffer
   (``max_receive_buffer`` / equivalent). If the reassembly buffer cannot
   hold the retained bytes, or a Length-declared frame exceeds
   ``max_receive_buffer``, the transport shall discard the buffered stream
   state for that connection and signal the application with
   ``Result::BUFFER_OVERFLOW`` through ``ITransportListener::on_message_rejected``
   (``MessageRejectionStage::TCP_FRAMING``), matching the threaded
   ``TcpTransport`` rejection path. The transport shall not retain complete
   unparsed PDUs to stall the peer, and shall not discard without a signal.

   **Rationale**: Open SOME/IP requires multi-message TCP segments
   (feat_req_someip_702) and notes TCP “exactly once” reliability with
   further error handling left to the application (feat_req_someip_435).
   The specification does not prescribe a local reassembly-buffer
   exhaustion policy; this requirement is an OpenSOMEIP implementation
   refinement of that TCP binding: reset the buffer and notify rather than
   silent drop or stalling complete frames.

   **Error Handling**: Clear the reassembly buffer; invoke
   ``on_message_rejected`` with ``BUFFER_OVERFLOW``; continue the connection
   without leaving complete unparsed frames buffered.

   **Code Location**: ``src/transport/event_driven_tcp_transport.cpp``
   (``on_adapter_receive``, ``parse_next_message``),
   ``src/transport/tcp_transport.cpp`` (``parse_next_message``)

.. requirement:: TCP Receive Message-Pool Exhaustion
   :id: REQ_TRANSPORT_029
   :satisfies: feat_req_someip_435
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Force ``allocate_message()`` failure for the first complete frame in a multi-frame TCP callback; verify that frame’s bytes are consumed (not left in the reassembly buffer), ``on_error(OUT_OF_MEMORY)`` is invoked once, later complete frames in the same callback are still delivered, and ``on_message_rejected`` is not used for this capacity case. Covered by AllocFailureConsumesFrameAndNotifiesOom.

   Message-pool exhaustion (``allocate_message()`` returning ``nullptr``) is a
   separate platform/PAL limit from reassembly-buffer exhaustion
   (``REQ_TRANSPORT_028``). On the event-driven TCP receive path, when a
   complete frame has been delimited but ``allocate_message()`` fails, the
   transport shall consume that frame’s bytes from the reassembly buffer,
   notify ``ITransportListener::on_error(Result::OUT_OF_MEMORY)``, and
   continue parsing any subsequent complete frames in the same callback.
   Complete frames shall not be left buffered to stall the peer. This path
   shall not use ``on_message_rejected`` for pool exhaustion (that hook is
   for structural PDU rejection per ``REQ_TRANSPORT_026``).

   **Rationale**: feat_req_someip_435 leaves further error handling to the
   application; Open SOME/IP does not define a Message-pool policy. This
   OpenSOMEIP rule aligns pool exhaustion with the consume-and-notify
   spirit of ``REQ_TRANSPORT_028`` while keeping the observable signal
   distinct (``on_error`` vs ``on_message_rejected``) because the failure
   is a PAL allocation limit, not a Length/framing buffer limit.
   ``REQ_PAL_MEM_EXHAUST_E01`` defines the pool’s ``nullptr`` contract.

   **Error Handling**: Consume the current complete frame; call
   ``on_error(OUT_OF_MEMORY)``; continue parsing.

   **Code Location**: ``src/transport/event_driven_tcp_transport.cpp``
   (``on_adapter_receive``, ``parse_next_message``)


Magic Cookie Details
====================

.. requirement:: Magic Cookie Message Format
   :id: REQ_TRANSPORT_025
   :satisfies: feat_req_someip_589, feat_req_someip_607
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Construct Magic Cookie and verify: Service ID=0xFFFF, Method ID=0x0000 (client) or 0x8000 (server), Length=8, Client ID=0xDEAD, Session ID=0xBEEF.

   The software shall use the specified Magic Cookie message layout:
   Service ID 0xFFFF, Method ID 0x0000/0x8000, Length 8, Client ID
   0xDEAD, Session ID 0xBEEF.

   **Rationale**: A well-defined Magic Cookie format ensures both peers can detect the resync marker.

   **Code Location**: ``src/transport/tcp_transport.cpp``


.. requirement:: Incoming Message Rejection Diagnostics
   :id: REQ_TRANSPORT_026
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit/integration test: Malformed UDP datagram and complete malformed TCP frame each produce one ``on_message_rejected`` notification and no ``on_message_received``. Incomplete TCP frames do not notify. Parsed Message ID / Request ID are present only when the header bytes were available. Existing listeners that do not override the default remain source-compatible. Covered by MalformedDatagramNotifiesRejectionNotMessage, ParseNextMessageDistinguishesOutcomes, CompleteMalformedFrameNotifiesRejection, IncompleteHeaderDoesNotNotifyRejection, ServerRejectionHandlerSeesMalformedRequest.

   The software shall expose an additive, defaulted transport hook for
   structural rejection of a complete incoming PDU. The hook shall carry
   the sender endpoint, a local ``Result``, a rejection stage, and Message
   ID / Request ID only when parsing reached those fields. UDP and TCP
   transports shall invoke the hook; RPC and event public APIs shall
   propagate it to an application-registerable callback. Incomplete TCP
   frames and consumed Magic Cookies shall not be reported as rejections.
   The hook is local diagnostics only and does not define a protocol
   error response.

   **Rationale**: Silent deserialize failure makes a rejecting peer look idle.

   **Error Handling**: Notify the listener/callback; do not deliver the PDU
   as an application message.

   **Code Location**: ``include/transport/message_rejection.h``,
   ``include/transport/transport.h``, ``src/transport/udp_transport.cpp``,
   ``src/transport/tcp_transport.cpp``,
   ``src/transport/event_driven_tcp_transport.cpp``,
   ``src/rpc/rpc_server.cpp``,
   ``src/rpc/rpc_client.cpp``, ``src/events/event_subscriber.cpp``,
   ``src/events/event_publisher.cpp``

.. requirement:: Error - UDP Send Failure
   :id: REQ_TRANSPORT_001_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Simulate sendto() returning ENETUNREACH, verify error is returned and logged.

   The software shall return an error and log details when UDP send fails.

   **Rationale**: Network unreachable and other send errors must be reported for diagnostics.

   **Error Handling**: Return SEND_FAILED error code with errno details.

   **Code Location**: ``src/transport/udp_transport.cpp``

.. requirement:: Error - UDP Bind Failure
   :id: REQ_TRANSPORT_001_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Attempt to bind to port already in use, verify error is returned with port number in log.

   The software shall return an error when UDP socket binding fails.

   **Rationale**: Port conflicts must be reported clearly for deployment troubleshooting.

   **Error Handling**: Return BIND_FAILED error code.

   **Code Location**: ``src/transport/udp_transport.cpp``

.. requirement:: Error - TCP Connection Refused
   :id: REQ_TRANSPORT_002_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Connect to non-listening port, verify ECONNREFUSED is reported and reconnection is scheduled.

   The software shall handle TCP connection refused by reporting the error and scheduling reconnection.

   **Rationale**: Connection refused indicates the server is not yet available.

   **Error Handling**: Log error, schedule reconnection with configurable backoff.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: Error - TCP Connection Reset
   :id: REQ_TRANSPORT_002_E02
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Simulate TCP RST during active communication, verify connection state transitions to DISCONNECTED and pending requests timeout.

   The software shall handle TCP connection resets by transitioning to DISCONNECTED and timing out pending requests.

   **Rationale**: Abrupt connection resets must trigger proper cleanup.

   **Error Handling**: Transition to DISCONNECTED, timeout pending requests, log reset.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: Error - Multicast Join Failure
   :id: REQ_TRANSPORT_011_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Join a multicast group with the interface set to a TEST-NET-1 address (RFC 5737) so IP_ADD_MEMBERSHIP fails independently of host multicast support; verify MULTICAST_ERROR is returned and the group and interface are retrievable via ``UdpTransport::last_multicast_error()``. Covered by MulticastJoinFailureIsReportedWithDiagnostics.

   The software shall return an error when multicast group join fails.

   **Rationale**: Failed multicast joins prevent SD and event reception.

   **Error Handling**: Return ``Result::MULTICAST_ERROR`` and record group address
   and interface. The project has no logging facility, so the context is exposed
   through ``UdpTransport::last_multicast_error()`` rather than written to a log.
   ``leave_multicast_group`` returns the same result class on membership failure.

   **Code Location**: ``src/transport/udp_transport.cpp``

.. requirement:: Error - Multicast Join Recovery
   :id: REQ_TRANSPORT_011_E03
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Exercise ``MulticastMembership`` fail-once-then-succeed, exhaustion, zero-bound disable, and leave-cancels-pending. SD server: configure TEST-NET-1 multicast interface, ``multicast_rejoin_max_attempts=1``, verify re-attempt on the offer timer then Exhausted. Covered by MulticastMembership unit tests and ServerMulticastRejoinIsBoundedAndReportsExhaustion.

   The software shall re-attempt a failed multicast group join a bounded number
   of times and report exhaustion. Pending re-attempts shall be cancelled when
   the group is left or ownership (subscription) is released. Client eventgroup
   membership tracking shall be bounded and keyed by owned subscriptions so a
   peer cannot grow pending state without a local subscription.

   **Rationale**: Reporting a join failure without recovery leaves the caller
   holding an error it cannot act on. REQ_TRANSPORT_002_E01 already pairs error
   reporting with scheduled recovery for the analogous TCP failure, and
   REQ_TRANSPORT_016_E01 bounds that recovery.

   **Error Handling**: Re-attempt from existing SD periodic work up to
   ``SdConfig::multicast_rejoin_max_attempts``, then report ``Exhausted``.
   Zero disables re-attempt.

   **Code Location**: ``include/sd/multicast_membership.h``,
   ``src/sd/sd_server.cpp``, ``src/sd/sd_client.cpp``

.. requirement:: Error - Port Already In Use
   :id: REQ_TRANSPORT_014_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Configure two services on the same port, verify second bind fails with clear error message.

   The software shall detect and report port conflicts during service startup.

   **Rationale**: Port conflicts prevent service startup and must be caught early.

   **Error Handling**: Return BIND_FAILED with port number in error message.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``src/transport/tcp_transport.cpp``

.. requirement:: Error - TCP Reconnection Exhaustion
   :id: REQ_TRANSPORT_016_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Configure max_reconnect_attempts=3, fail all 3, verify RECONNECT_EXHAUSTED error and connection is abandoned.

   The software shall stop reconnection attempts after the configured maximum and report failure.

   **Rationale**: Unbounded reconnection wastes resources when the server is permanently unavailable.

   **Error Handling**: Report RECONNECT_EXHAUSTED, transition to DISCONNECTED.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: Error - TCP Message Framing Error
   :id: REQ_TRANSPORT_002_E03
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Inject corrupted data in TCP stream (invalid length field), verify stream resynchronization via Magic Cookie or connection reset.

   The software shall handle TCP framing errors by attempting resynchronization or resetting the connection.

   **Rationale**: Framing errors corrupt the TCP stream and prevent further message parsing.

   **Error Handling**: Attempt Magic Cookie resync if available, otherwise reset connection.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: Error - UDP Receive Buffer Too Small
   :id: REQ_TRANSPORT_001_E03
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Configure receive buffer smaller than incoming message, verify message is truncated and warning logged.

   The software shall handle UDP receive buffer truncation by logging a warning.

   **Rationale**: Truncated UDP messages cannot be processed correctly.

   **Error Handling**: Log warning with received vs expected size, discard message.

   **Code Location**: ``src/transport/udp_transport.cpp``

.. requirement:: Error - Endpoint Address Format Invalid
   :id: REQ_TRANSPORT_006_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Configure endpoint with invalid IP 'abc.def.ghi.jkl', verify validation error on startup.

   The software shall validate endpoint address format during configuration and report errors.

   **Rationale**: Invalid addresses prevent communication and must be caught at configuration time.

   **Error Handling**: Return INVALID_ENDPOINT error with address string.

   **Code Location**: ``include/transport/endpoint.h``

.. requirement:: Error - TCP Server Socket Exhaustion
   :id: REQ_TRANSPORT_003_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Set max_connections=5, attempt 6th connection, verify rejection with appropriate error response.

   The software shall reject new TCP connections when the maximum connection limit is reached.

   **Rationale**: Connection limits prevent resource exhaustion from excessive clients.

   **Error Handling**: Reject connection, log client address and current connection count.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: Error - TCP Send on Disconnected Socket
   :id: REQ_TRANSPORT_002_E04
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Disconnect TCP transport, attempt send, verify SEND_FAILED error with clear message.

   The software shall return an error when attempting to send on a disconnected TCP connection.

   **Rationale**: Send on disconnected socket causes undefined behavior.

   **Error Handling**: Return SEND_FAILED, queue message for retry after reconnect.

   **Code Location**: ``src/transport/tcp_transport.cpp``

.. requirement:: Error - UDP Multicast TTL Configuration
   :id: REQ_TRANSPORT_011_E02
   :status: implemented
   :priority: low
   :category: error_path
   :verification: Unit test: Configure multicast TTL=0, verify multicast messages do not leave the host. TTL=1, verify single hop.

   The software shall configure the multicast TTL to limit message propagation scope.

   **Rationale**: Incorrect TTL causes multicast messages to leak across network boundaries.

   **Error Handling**: Apply configured TTL, default to 1 if not specified.

   **Code Location**: ``src/transport/udp_transport.cpp``

Traceability
============

Implementation Files
--------------------

* ``include/transport/transport.h`` - Transport interface (REQ_TRANSPORT_005)
* ``include/transport/message_rejection.h`` - Incoming PDU rejection diagnostics
* ``include/transport/udp_transport.h`` - UDP transport interface
* ``include/transport/tcp_transport.h`` - TCP transport interface
* ``include/transport/event_driven_udp_transport.h`` - Event-driven UDP transport (REQ_TRANSPORT_001a, REQ_TRANSPORT_001b, REQ_TRANSPORT_005, REQ_TRANSPORT_011)
* ``include/transport/event_driven_tcp_transport.h`` - Event-driven TCP transport (REQ_TRANSPORT_024, REQ_TRANSPORT_027, REQ_TRANSPORT_028, REQ_TRANSPORT_029)
* ``include/transport/udp_socket_adapter.h`` - UDP socket adapter for event-driven path (REQ_TRANSPORT_001a, REQ_TRANSPORT_001b, REQ_TRANSPORT_011)
* ``include/transport/tcp_socket_adapter.h`` - TCP socket adapter for event-driven path (REQ_TRANSPORT_024, REQ_TRANSPORT_027)
* ``include/transport/multicast_transport.h`` - Multicast join/leave interface (REQ_TRANSPORT_001b, REQ_TRANSPORT_011)
* ``include/sd/multicast_membership.h`` - Bounded multicast join retry state (REQ_TRANSPORT_011_E01, REQ_TRANSPORT_011_E03)
* ``include/transport/endpoint.h`` - Endpoint structure
* ``src/transport/udp_transport.cpp`` - UDP implementation
* ``src/transport/tcp_transport.cpp`` - TCP implementation
* ``src/transport/event_driven_udp_transport.cpp`` - Event-driven UDP implementation (REQ_TRANSPORT_001a, REQ_TRANSPORT_001b, REQ_TRANSPORT_005, REQ_TRANSPORT_011)
* ``src/transport/event_driven_tcp_transport.cpp`` - Event-driven TCP implementation

Test Files
----------

* ``tests/test_udp_transport.cpp`` - UDP transport tests
* ``tests/test_multicast_membership.cpp`` - Multicast membership retry state machine tests
* ``tests/test_tcp_transport.cpp`` - TCP transport tests
* ``tests/test_event_driven_udp_transport.cpp`` - Event-driven UDP transport tests (REQ_TRANSPORT_001a, REQ_TRANSPORT_001b, REQ_TRANSPORT_011)
* ``tests/test_event_driven_tcp_transport.cpp`` - Event-driven TCP transport tests

Examples
--------

* ``examples/basic/hello_world_client.cpp`` - Basic UDP client
* ``examples/basic/hello_world_server.cpp`` - Basic UDP server
* ``examples/advanced/tcp_client.cpp`` - TCP client example
* ``examples/advanced/tcp_server.cpp`` - TCP server example
