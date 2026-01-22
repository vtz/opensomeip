..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
OpenSOMEIP Requirements
==============================

This documentation defines the requirements for the OpenSOMEIP implementation.
Requirements are traced to the Open SOME/IP Specification and linked to code
and test cases for full bidirectional traceability.

Overview
========

The OpenSOMEIP requirements are organized into the following categories:

1. **Specification Requirements**: Imported from the Open SOME/IP Specification
2. **Implementation Requirements**: OpenSOMEIP-specific requirements that satisfy spec requirements
3. **Test Cases**: Test coverage mapped to requirements
4. **Code References**: Code locations implementing requirements

Traceability Model
==================

::

    open-someip-spec Requirements (feat_req_someip_*)
      ↓ satisfies
    OpenSOMEIP Requirements (REQ_*)
      ↓ implements
    Code Locations (src/**/*.cpp, include/**/*.h)
      ↓ tested_by
    Test Cases (tests/**/*.cpp, tests/**/*.py)

Contents
========

.. toctree::
   :maxdepth: 2
   :caption: Requirements

   spec_references
   implementation/e2e_plugin
   implementation/transport
   implementation/architecture

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
* :ref:`needsindex`
