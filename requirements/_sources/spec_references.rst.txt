..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
Specification Requirements
==============================

This section references requirements from the Open SOME/IP Specification.
These requirements are imported from the ``open-someip-spec`` repository
and are not duplicated here.

.. note::

   Specification requirements are imported via ``needs_import`` from
   ``open-someip-spec/build/needs.json``. Build the spec first to
   generate the needs JSON file.

Key Specification Requirements
==============================

The following specification requirements are particularly relevant to
the OpenSOMEIP implementation:

E2E Protection
--------------

* ``feat_req_someip_102``: E2E header insertion
* ``feat_req_someip_103``: E2E header format

Message Format
--------------

* ``feat_req_someip_538`` - ``feat_req_someip_549``: Message header structure

Serialization
-------------

* ``feat_req_someip_600`` - ``feat_req_someip_622``: Data type serialization

Service Discovery
-----------------

* ``feat_req_someipsd_100`` - ``feat_req_someipsd_320``: SD protocol

Transport Protocol
------------------

* ``feat_req_someiptp_400`` - ``feat_req_someiptp_414``: TP segmentation

Imported Requirements
=====================

.. note::

   When the Open SOME/IP Specification is built, all ``feat_req_*``
   requirements will be available for linking via the ``satisfies``
   option. Use the following syntax in your requirements:

   .. code-block:: rst

      .. requirement:: My Requirement
         :id: REQ_MY_001
         :satisfies: feat_req_someip_102, feat_req_someip_103

         This requirement satisfies spec requirements.

Viewing Imported Requirements
=============================

To view imported specification requirements, build the open-someip-spec
and then build this documentation:

.. code-block:: bash

   # Build spec to generate needs.json
   cd open-someip-spec
   pip install -r requirements.txt
   sphinx-build -b needs src build

   # Build requirements docs
   cd ../docs/requirements
   sphinx-build -b html . _build/html

The imported requirements will be available in the needs index.
