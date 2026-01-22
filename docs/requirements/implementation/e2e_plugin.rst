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

   The E2E header shall be inserted after the Return Code field in the
   SOME/IP header, with its position determined by the configured offset
   value (default: 64 bits = 8 bytes).

   The standard E2E header format shall be:

   * CRC: 32 bits
   * Counter: 32 bits
   * Data ID: 16 bits
   * Freshness Value: 16 bits
   * Total: 12 bytes (96 bits)

   **Rationale**: Complies with SOME/IP specification feat_req_someip_102
   and feat_req_someip_103.

   **Code Location**: ``include/e2e/e2e_header.h``

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
