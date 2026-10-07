..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
E2E Plugin Mechanism
==============================

This section defines requirements for the End-to-End (E2E) protection
plugin mechanism in OpenSOMEIP. The plugin mechanism allows external
E2E profiles (e.g., AUTOSAR profiles) to be integrated without modifying
the core implementation.

Overview
========

The E2E protection mechanism provides:

1. A plugin interface for custom E2E profiles
2. A registry for managing registered profiles
3. A standard profile using public standards (SAE-J1850, ITU-T X.25)

Requirements
============

Plugin Interface
----------------

.. requirement:: E2E Profile Plugin Interface
   :id: REQ_E2E_PLUGIN_001
   :satisfies: feat_req_someip_102, feat_req_someip_103
   :status: implemented
   :priority: high
   :verification: Code inspection of abstract interface definition and successful compilation of external profile implementations.

   The implementation shall provide an abstract plugin interface (``E2EProfile``)
   that allows external E2E protection profiles to be integrated.

   The interface shall define the following methods:

   * ``protect(Message&, E2EConfig&)``: Add E2E protection to a message
   * ``validate(const Message&, E2EConfig&)``: Validate E2E protection
   * ``get_profile_id()``: Return unique profile identifier
   * ``get_profile_name()``: Return profile name

   **Rationale**: Allows AUTOSAR or custom E2E profiles to be provided as
   external libraries without modifying the core implementation.

   **Code Location**: ``include/e2e/e2e_profile.h``

Profile Registry
----------------

.. requirement:: E2E Profile Registry
   :id: REQ_E2E_PLUGIN_002
   :status: implemented
   :priority: high
   :verification: Execution of registry unit tests demonstrating profile registration, lookup, and singleton behavior.

   The implementation shall provide a registry (``E2EProfileRegistry``)
   for managing E2E protection profiles.

   The registry shall:

   * Be a singleton accessible via ``E2EProfileRegistry::instance()``
   * Support registration of profiles via ``register_profile()``
   * Support lookup by profile ID via ``get_profile(uint32_t)``
   * Support lookup by profile name via ``get_profile(string)``
   * Provide a default profile via ``get_default_profile()``

   **Rationale**: Centralized management of E2E profiles enables
   runtime selection and configuration.

   **Code Location**: ``include/e2e/e2e_profile_registry.h``

Plugin Registration API
-----------------------

.. requirement:: Plugin Registration API
   :id: REQ_E2E_PLUGIN_003
   :status: implemented
   :priority: high
   :verification: Execution of registration API tests demonstrating ownership transfer, duplicate prevention, and successful plugin loading.

   The implementation shall provide an API for registering E2E profiles
   at runtime.

   The API shall:

   * Accept ``std::unique_ptr<E2EProfile>`` for ownership transfer
   * Return success/failure status
   * Prevent duplicate registrations (same ID or name)
   * Support unregistration if needed

   **Rationale**: Enables dynamic loading and registration of E2E
   profile plugins.

   **Code Location**: ``src/e2e/e2e_profile_registry.cpp``

Standard Profile
----------------

.. requirement:: Standard E2E Profile
   :id: REQ_E2E_PLUGIN_004
   :satisfies: feat_req_someip_102, feat_req_someip_103
   :status: implemented
   :priority: high
   :verification: Execution of E2E protection tests demonstrating CRC calculation, counter validation, and data integrity protection.

   The implementation shall provide a standard E2E profile using
   publicly available standards:

   * CRC8 using SAE-J1850 polynomial
   * CRC16 using ITU-T X.25 (CCITT) polynomial
   * CRC32 using ISO 3309 polynomial
   * Counter mechanism for replay detection
   * Freshness value for stale data detection
   * Data ID for message identification

   **Rationale**: Provides E2E protection without requiring proprietary
   AUTOSAR profiles.

   **Code Location**: ``src/e2e/e2e_profiles/standard_profile.cpp``

E2E Header Format
-----------------

.. requirement:: E2E Header Format
   :id: REQ_E2E_PLUGIN_005
   :satisfies: feat_req_someip_102, feat_req_someip_103
   :status: implemented
   :priority: high
   :verification: Code inspection of header structure and execution of message serialization tests with E2E headers.

   ``E2EConfig::offset_bits`` is the spec Offset, measured in bits from the
   start of the Length-covered region (Request ID). The default is 64 bits,
   which places the header immediately after Return Code (wire byte 16).

   A byte-aligned Offset greater than 64 is supported. The header starts at
   wire byte ``8 + offset_bits/8``. Bytes between Return Code and that index
   are an unprotected prefix: they are stored apart from ``Message::get_payload()``,
   they are not covered by the basic-profile CRC, and they are included in
   the Length field together with the E2E header and the application payload
   (feat_req_someip_77). The caller supplies the prefix with
   ``Message::set_e2e_unprotected_prefix`` before ``protect``. Deserialize
   takes the Offset and header size from ``E2EParseOptions`` (configuration).
   ``expect_e2e == true`` selects only the default layout. The receiver does
   not discover Offset or header size from the datagram.

   ``Message`` stores profile bytes in a fixed buffer of
   ``SOMEIP_MAX_E2E_HEADER_SIZE`` (default 64). The 12-byte ``E2EHeader``
   view is available only when the stored size is 12. Plugins may return any
   other size up to that cap. ``SOMEIP_MAX_E2E_PREFIX_SIZE`` (default 64)
   caps the unprotected prefix. Both caps are part of
   ``SOMEIP_MAX_MESSAGE_SIZE``.

   Error codes, evaluated in this order:

   * ``Result::NOT_INITIALIZED`` when no profile is registered
   * ``Result::INVALID_ARGUMENT`` when ``offset_bits < 64`` (overlaps the
     SOME/IP header) or ``offset_bits`` is not a multiple of 8, and for CRC,
     Data ID, replay, and other caller errors
   * ``Result::NOT_IMPLEMENTED`` when the prefix or the profile header
     exceeds its compile-time cap

   SOME/IP-TP and the default E2E layout both occupy wire byte 16. A message
   that already has an E2E header is not segmented, at any supported Offset
   or header size. Reassembly completes before E2E parse. The C ABI
   (``opensomeip_e2e_protect`` / ``opensomeip_e2e_check``) stays on the
   default layout and has no Offset field.

   The basic profile header format shall be:

   * CRC: 32 bits
   * Counter: 32 bits
   * Data ID: 16 bits
   * Freshness Value: 16 bits
   * Total: 12 bytes (96 bits)

   **Rationale**: feat_req_someip_102 places the header by Offset (default
   64 bits). feat_req_someip_103 allows the header size to depend on the
   profile. Bit packing and headers or prefixes above the static caps stay
   unrepresentable.

   **Code Location**: ``include/e2e/e2e_header.h``, ``include/e2e/e2e_layout.h``,
   ``src/e2e/e2e_protection.cpp``, ``src/someip/message.cpp``

Traceability
============

Implementation Files
--------------------

* ``include/e2e/e2e_profile.h`` - Profile interface
* ``include/e2e/e2e_profile_registry.h`` - Registry interface
* ``include/e2e/e2e_header.h`` - E2E header structure
* ``include/e2e/e2e_config.h`` - Configuration structure
* ``include/e2e/e2e_protection.h`` - Protection API
* ``src/e2e/e2e_profile_registry.cpp`` - Registry implementation
* ``src/e2e/e2e_protection.cpp`` - Protection implementation
* ``src/e2e/e2e_profiles/standard_profile.cpp`` - Standard profile

Test Files
----------

* ``tests/test_e2e.cpp`` - Unit tests
* ``tests/integration/test_e2e_integration.py`` - Integration tests
* ``tests/system/test_e2e_system.py`` - System tests

Examples
--------

* ``examples/e2e_protection/basic_e2e.cpp`` - Basic usage
* ``examples/e2e_protection/plugin_integration.cpp`` - Plugin example
* ``examples/e2e_protection/safety_critical.cpp`` - Safety-critical usage
