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

.. requirement:: UDP Transport Implementation
   :id: REQ_TRANSPORT_001
   :satisfies: feat_req_someip_800, feat_req_someip_801, feat_req_someip_802
   :status: implemented
   :priority: high
   :verification: Execution of UDP transport unit tests and successful message transmission/reception in integration tests.

   The implementation shall provide a UDP transport (``UdpTransport``)
   that supports:

   * Binding to local address and port
   * Sending messages to unicast and multicast destinations
   * Receiving messages from any source
   * Multicast group join/leave
   * Non-blocking I/O with configurable timeouts
   * Thread-safe operation

   **Rationale**: UDP is the primary transport for SOME/IP communication,
   including Service Discovery multicast.

   **Code Location**: ``src/transport/udp_transport.cpp``

TCP Transport
-------------

.. requirement:: TCP Transport Implementation
   :id: REQ_TRANSPORT_002
   :satisfies: feat_req_someip_850, feat_req_someip_851, feat_req_someip_852
   :status: implemented
   :priority: high
   :verification: Execution of TCP transport unit tests and successful client-server communication in integration tests.

   The implementation shall provide a TCP transport (``TcpTransport``)
   that supports:

   * Client mode: Connect to remote server
   * Server mode: Accept incoming connections
   * Message framing over TCP streams
   * Connection state management
   * Automatic reconnection (configurable)
   * Thread-safe operation

   **Rationale**: TCP provides reliable transport for scenarios requiring
   guaranteed delivery.

   **Code Location**: ``src/transport/tcp_transport.cpp``

Connection Management
---------------------

.. requirement:: Connection Management
   :id: REQ_TRANSPORT_003
   :satisfies: feat_req_someip_850, feat_req_someip_851
   :status: implemented
   :priority: medium
   :verification: Code inspection of connection state tracking and execution of connection lifecycle tests.

   The TCP transport shall implement connection management:

   * Track connection state (disconnected, connecting, connected)
   * Notify listeners of connection state changes
   * Support graceful connection shutdown
   * Handle connection errors and timeouts
   * Support multiple simultaneous connections (server mode)

   **Rationale**: Proper connection management is essential for
   reliable TCP communication.

   **Code Location**: ``src/transport/tcp_transport.cpp``

Error Recovery
--------------

.. requirement:: Transport Error Recovery
   :id: REQ_TRANSPORT_004
   :satisfies: feat_req_someip_800, feat_req_someip_850
   :status: implemented
   :priority: medium
   :verification: Execution of error handling tests and verification of proper error code propagation to application layer.

   The transport layer shall implement error recovery mechanisms:

   * Retry send operations on transient errors
   * Close and reopen sockets on persistent errors
   * Log errors with sufficient detail for diagnosis
   * Return appropriate error codes to callers
   * Support configurable retry policies

   **Rationale**: Robust error handling improves reliability in
   real-world network conditions.

   **Code Location**: ``src/transport/udp_transport.cpp``, ``src/transport/tcp_transport.cpp``

Transport Interface
-------------------

.. requirement:: Abstract Transport Interface
   :id: REQ_TRANSPORT_005
   :satisfies: feat_req_someip_800, feat_req_someip_850
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

   **Code Location**: ``include/transport/transport.h``

Endpoint Configuration
----------------------

.. requirement:: Endpoint Configuration
   :id: REQ_TRANSPORT_006
   :satisfies: feat_req_someip_800, feat_req_someip_850
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

Traceability
============

Implementation Files
--------------------

* ``include/transport/transport.h`` - Transport interface
* ``include/transport/udp_transport.h`` - UDP transport interface
* ``include/transport/tcp_transport.h`` - TCP transport interface
* ``include/transport/endpoint.h`` - Endpoint structure
* ``src/transport/udp_transport.cpp`` - UDP implementation
* ``src/transport/tcp_transport.cpp`` - TCP implementation

Test Files
----------

* ``tests/test_udp_transport.cpp`` - UDP transport tests
* ``tests/test_tcp_transport.cpp`` - TCP transport tests

Examples
--------

* ``examples/basic/hello_world_client.cpp`` - Basic UDP client
* ``examples/basic/hello_world_server.cpp`` - Basic UDP server
* ``examples/advanced/tcp_client.cpp`` - TCP client example
* ``examples/advanced/tcp_server.cpp`` - TCP server example
