..
   Copyright (c) 2025 Vinicius Tadeu Zein

   See the NOTICE file(s) distributed with this work for additional
   information regarding copyright ownership.

   This program and the accompanying materials are made available under the
   terms of the Apache License Version 2.0 which is available at
   https://www.apache.org/licenses/LICENSE-2.0

   SPDX-License-Identifier: Apache-2.0

==============================
Service Discovery Requirements
==============================

This section defines Software Low-Level Requirements (SW-LLR) for the
SOME/IP Service Discovery (SOME/IP-SD) module. SD is used to locate
service instances, detect their availability, and implement Publish/Subscribe.

Overview
========

The Service Discovery module handles:

1. SD message parsing and generation
2. Service offer and find operations
3. Eventgroup subscription management
4. TTL-based service availability tracking
5. Reboot detection

SD Message Format
=================

Header Requirements
-------------------

.. requirement:: SD Service ID
   :id: REQ_SD_001
   :satisfies: feat_req_someipsd_141
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD messages use Service ID 0xFFFF.

   The software shall use Service ID 0xFFFF for all SOME/IP-SD messages.

   **Rationale**: Service ID 0xFFFF is reserved for Service Discovery.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: SD Method ID
   :id: REQ_SD_002
   :satisfies: feat_req_someipsd_142
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD messages use Method ID 0x8100.

   The software shall use Method ID 0x8100 for all SOME/IP-SD messages.

   **Rationale**: Method ID 0x8100 is defined for SD messages.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: SD Client ID
   :id: REQ_SD_003
   :satisfies: feat_req_someipsd_144
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD messages use Client ID 0x0000.

   The software shall use Client ID 0x0000 for all SOME/IP-SD messages.

   **Rationale**: Client ID 0x0000 is reserved for SD.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: SD Protocol Version
   :id: REQ_SD_004
   :satisfies: feat_req_someipsd_147
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD messages use Protocol Version 0x01.

   The software shall use Protocol Version 0x01 for all SOME/IP-SD messages.

   **Rationale**: SOME/IP-SD uses Protocol Version 1.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: SD Interface Version
   :id: REQ_SD_005
   :satisfies: feat_req_someipsd_148
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD messages use Interface Version 0x01.

   The software shall use Interface Version 0x01 for all SOME/IP-SD messages.

   **Rationale**: SOME/IP-SD uses Interface Version 1.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: SD Message Type
   :id: REQ_SD_006
   :satisfies: feat_req_someipsd_205
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD messages use Message Type 0x02 (NOTIFICATION).

   The software shall use Message Type 0x02 (NOTIFICATION) for all
   SOME/IP-SD messages.

   **Rationale**: SD messages are notifications.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: SD Return Code
   :id: REQ_SD_007
   :satisfies: feat_req_someipsd_208
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD messages use Return Code 0x00 (E_OK).

   The software shall use Return Code 0x00 (E_OK) for all SOME/IP-SD messages.

   **Rationale**: SD messages always use E_OK return code.

   **Code Location**: ``src/sd/sd_message.cpp``

SD Flags Parsing
----------------

.. requirement:: Parse SD Flags Byte
   :id: REQ_SD_010
   :satisfies: feat_req_someipsd_209
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify SD Flags byte is correctly parsed from byte 0 of SD payload.

   The software shall parse the SD Flags byte from the first byte (byte 0)
   of the SOME/IP-SD payload.

   **Rationale**: Flags byte contains control flags for SD processing.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract Reboot Flag
   :id: REQ_SD_011
   :satisfies: feat_req_someipsd_213
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Reboot flag is extracted from bit 7 of Flags byte.

   The software shall extract the Reboot flag from bit 7 (most significant bit)
   of the SD Flags byte.

   **Rationale**: Reboot flag indicates ECU reboot for session management.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract Unicast Flag
   :id: REQ_SD_012
   :satisfies: feat_req_someipsd_213
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Unicast flag is extracted from bit 6 of Flags byte.

   The software shall extract the Unicast flag from bit 6 of the SD
   Flags byte.

   **Rationale**: Unicast flag indicates unicast-capable endpoint.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Validate Reserved Flags
   :id: REQ_SD_013
   :satisfies: feat_req_someipsd_213
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify reserved flag bits 5-0 are zero on transmit.

   The software shall set reserved flag bits (bits 5-0) to zero when
   generating SD messages.

   **Rationale**: Reserved bits must be zero per specification.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Ignore Reserved Flags on Receive
   :id: REQ_SD_014
   :satisfies: feat_req_someipsd_148
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify reserved flag bits are ignored during parsing.

   The software shall ignore reserved flag bits (bits 5-0) when parsing
   received SD messages.

   **Rationale**: Forward compatibility with future flag definitions.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Invalid SD Header
   :id: REQ_SD_001_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify messages not matching SD header requirements are rejected.

   The software shall reject messages that do not match the required
   SD header values (Service ID 0xFFFF, Method ID 0x8100).

   **Rationale**: Invalid SD header indicates malformed message.

   **Error Handling**: Discard message and log error.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Malformed SD Flags
   :id: REQ_SD_010_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when SD payload is too short for Flags byte.

   The software shall return an error when the SD payload is too short
   to contain the Flags byte.

   **Rationale**: Minimum SD payload must contain Flags and Reserved bytes.

   **Error Handling**: Return MALFORMED_MESSAGE error.

   **Code Location**: ``src/sd/sd_message.cpp``

Entry Parsing
=============

Entries Array
-------------

.. requirement:: Parse Entries Length Field
   :id: REQ_SD_020
   :satisfies: feat_req_someipsd_575
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Entries Length field is parsed from bytes 4-7 of SD payload.

   The software shall parse the Entries Length field as a 4-byte Big Endian
   value from bytes 4-7 of the SOME/IP-SD payload.

   **Rationale**: Entries Length indicates the size of the entries array.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Entry Type
   :id: REQ_SD_021
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Entry Type is correctly parsed from first byte of entry.

   The software shall parse the Entry Type from the first byte of each
   SD entry.

   **Rationale**: Entry Type determines the entry format and semantics.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Entry Size Calculation
   :id: REQ_SD_022
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify entry size is 16 bytes for service entries.

   The software shall use an entry size of 16 bytes for service-type
   entries (Type 0, Type 1).

   **Rationale**: Fixed entry size per specification.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Type 0 Find Service Entry
   :id: REQ_SD_023
   :satisfies: feat_req_someipsd_626
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Type 0 (Find Service) entries are correctly parsed.

   The software shall parse Entry Type 0 as a Find Service entry,
   extracting Service ID, Instance ID, Major Version, and TTL.

   **Rationale**: Find Service is used by clients to discover services.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Type 1 Offer Service Entry
   :id: REQ_SD_024
   :satisfies: feat_req_someipsd_626
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Type 1 (Offer Service) entries are correctly parsed.

   The software shall parse Entry Type 1 as an Offer Service entry,
   extracting Service ID, Instance ID, Major Version, Minor Version, and TTL.

   **Rationale**: Offer Service announces service availability.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Type 6 Subscribe Eventgroup Entry
   :id: REQ_SD_025
   :satisfies: feat_req_someipsd_629
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Type 6 (Subscribe Eventgroup) entries are correctly parsed.

   The software shall parse Entry Type 6 as a Subscribe Eventgroup entry,
   extracting Service ID, Instance ID, Eventgroup ID, Major Version, and TTL.

   **Rationale**: Subscribe Eventgroup requests event notifications.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Type 7 Subscribe Eventgroup Ack Entry
   :id: REQ_SD_026
   :satisfies: feat_req_someipsd_630
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Type 7 (Subscribe Eventgroup Ack) entries are correctly parsed.

   The software shall parse Entry Type 7 as a Subscribe Eventgroup Ack entry,
   extracting Service ID, Instance ID, Eventgroup ID, and Reserved/Counter.

   **Rationale**: Subscribe Eventgroup Ack confirms or denies subscription.

   **Code Location**: ``src/sd/sd_message.cpp``

Entry Fields
------------

.. requirement:: Parse Service ID in Entry
   :id: REQ_SD_030
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Service ID is parsed from bytes 4-5 of entry.

   The software shall parse the Service ID from bytes 4-5 of each SD entry
   in Big Endian byte order.

   **Rationale**: Service ID identifies the service.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Instance ID in Entry
   :id: REQ_SD_031
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Instance ID is parsed from bytes 6-7 of entry.

   The software shall parse the Instance ID from bytes 6-7 of each SD entry
   in Big Endian byte order.

   **Rationale**: Instance ID identifies the service instance.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Instance ID Wildcard
   :id: REQ_SD_032
   :satisfies: feat_req_someipsd_734
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Instance ID 0xFFFF matches all instances.

   The software shall interpret Instance ID 0xFFFF as a wildcard matching
   all service instances.

   **Rationale**: Wildcard for discovering any instance of a service.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Major Version and TTL
   :id: REQ_SD_033
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Major Version (1 byte) and TTL (3 bytes) are parsed correctly.

   The software shall parse the combined Major Version (1 byte) and TTL
   (3 bytes) from bytes 8-11 of each SD entry in Big Endian byte order.

   **Rationale**: Major Version and TTL share a 4-byte field.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Minor Version
   :id: REQ_SD_034
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Minor Version is parsed from bytes 12-15 of Offer entry.

   The software shall parse the Minor Version from bytes 12-15 of Offer
   Service entries in Big Endian byte order.

   **Rationale**: Minor Version indicates compatible version range.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Eventgroup ID
   :id: REQ_SD_035
   :satisfies: feat_req_someipsd_629
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Eventgroup ID is parsed from bytes 14-15 of subscription entry.

   The software shall parse the Eventgroup ID from bytes 14-15 of
   Subscribe Eventgroup entries in Big Endian byte order.

   **Rationale**: Eventgroup ID identifies the subscribed event group.

   **Code Location**: ``src/sd/sd_message.cpp``

Entry Error Handling
--------------------

.. requirement:: Error - Unknown Entry Type
   :id: REQ_SD_021_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify unknown entry types are skipped with warning.

   The software shall skip entries with unknown Entry Type values and
   log a warning, continuing to process remaining entries.

   **Rationale**: Forward compatibility with new entry types.

   **Error Handling**: Log warning and skip entry.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Truncated Entry
   :id: REQ_SD_022_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when entry extends beyond Entries Length.

   The software shall return an error when an entry would extend beyond
   the declared Entries Length boundary.

   **Rationale**: Prevents buffer overread.

   **Error Handling**: Return MALFORMED_MESSAGE error.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Invalid Entries Length
   :id: REQ_SD_020_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when Entries Length exceeds remaining payload.

   The software shall return an error when the Entries Length field
   exceeds the remaining SD payload size.

   **Rationale**: Prevents buffer overread.

   **Error Handling**: Return MALFORMED_MESSAGE error.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Entries Length Not Multiple of Entry Size
   :id: REQ_SD_020_E02
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify warning when Entries Length is not a multiple of entry size.

   The software shall log a warning when the Entries Length is not an
   exact multiple of the entry size.

   **Rationale**: May indicate malformed message or version mismatch.

   **Error Handling**: Log warning and process complete entries.

   **Code Location**: ``src/sd/sd_message.cpp``

TTL Processing
==============

.. requirement:: Store Service Offer TTL
   :id: REQ_SD_040
   :satisfies: feat_req_someipsd_748
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TTL is stored for each received service offer.

   The software shall store the TTL value for each service offer received,
   associated with the Service ID and Instance ID.

   **Rationale**: TTL tracks service availability duration.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Decrement TTL Periodically
   :id: REQ_SD_041
   :satisfies: feat_req_someipsd_748
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TTL is decremented every second.

   The software shall decrement the stored TTL value for each active
   service offer every second.

   **Rationale**: TTL countdown for service expiration.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Remove Service on TTL Expiry
   :id: REQ_SD_042
   :satisfies: feat_req_someipsd_748
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify service is removed from available list when TTL reaches 0.

   The software shall remove a service from the available services list
   when its TTL value reaches zero.

   **Rationale**: Expired services are no longer considered available.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Infinite TTL Value
   :id: REQ_SD_043
   :satisfies: feat_req_someipsd_748
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TTL 0xFFFFFF (16777215) indicates infinite lifetime.

   The software shall interpret TTL value 0xFFFFFF (16777215 seconds) as
   an infinite lifetime that shall not be decremented.

   **Rationale**: Special value for permanent service availability.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Stop Offer TTL Zero
   :id: REQ_SD_044
   :satisfies: feat_req_someipsd_748
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify TTL 0 causes immediate service removal.

   The software shall interpret TTL value 0 as an immediate Stop Offer,
   removing the service from the available list without delay.

   **Rationale**: TTL 0 signals service is no longer available.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Reset TTL on New Offer
   :id: REQ_SD_045
   :satisfies: feat_req_someipsd_748
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify new offer resets TTL for existing service entry.

   The software shall reset the TTL to the new value when a new Offer
   is received for an already-known service instance.

   **Rationale**: Refreshes service availability on repeated offers.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Notify Application on Service Availability Change
   :id: REQ_SD_046
   :satisfies: feat_req_someipsd_14
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify callback is invoked when service becomes available/unavailable.

   The software shall notify the application layer when a service becomes
   available (offer received) or unavailable (TTL expired or stop offer).

   **Rationale**: Application needs to know service availability.

   **Code Location**: ``src/sd/sd_server.cpp``

TTL Error Handling
------------------

.. requirement:: Error - TTL Underflow Protection
   :id: REQ_SD_041_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify TTL decrement handles underflow correctly.

   The software shall prevent TTL underflow by not decrementing TTL
   values that are already zero.

   **Rationale**: Prevents unsigned integer underflow.

   **Error Handling**: Skip decrement for TTL = 0.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Error - Service List Full
   :id: REQ_SD_040_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify error when maximum service entries is reached.

   The software shall return an error or drop oldest entry when the
   maximum number of tracked services is reached.

   **Rationale**: Resource management for memory-constrained systems.

   **Error Handling**: Return RESOURCE_EXHAUSTED or apply eviction policy.

   **Code Location**: ``src/sd/sd_server.cpp``

Reboot Detection
================

.. requirement:: Track Session ID per Endpoint
   :id: REQ_SD_050
   :satisfies: feat_req_someipsd_795
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Session ID is stored per remote endpoint.

   The software shall store the Session ID from SD messages, associated
   with the source endpoint (IP address and port).

   **Rationale**: Session ID tracking enables reboot detection.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Compare Session ID for Reboot Detection
   :id: REQ_SD_051
   :satisfies: feat_req_someipsd_795
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify new Session ID is compared against stored value.

   The software shall compare the Session ID in newly received SD messages
   against the stored Session ID for the source endpoint.

   **Rationale**: Session ID regression indicates reboot.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Detect Reboot on Session ID Regression
   :id: REQ_SD_052
   :satisfies: feat_req_someipsd_795
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reboot is detected when new Session ID < stored (accounting for wrap).

   The software shall detect a reboot condition when the new Session ID
   is less than the stored Session ID (not within wrap-around threshold).

   **Rationale**: Lower Session ID indicates ECU restarted.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Detect Reboot on Reboot Flag
   :id: REQ_SD_053
   :satisfies: feat_req_someipsd_795
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reboot is detected when Reboot flag is set.

   The software shall detect a reboot condition when the Reboot flag
   (bit 7 of SD Flags) is set in a received SD message.

   **Rationale**: Explicit reboot indication from sender.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Trigger Reboot Detection Event
   :id: REQ_SD_054
   :satisfies: feat_req_someipsd_795
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify reboot detection event is triggered.

   The software shall trigger a reboot detection event when a reboot
   condition is detected for a remote endpoint.

   **Rationale**: Application may need to handle reconnection.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Clear Cached Services on Reboot
   :id: REQ_SD_055
   :satisfies: feat_req_someipsd_795
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify all cached services from rebooted endpoint are cleared.

   The software shall clear all cached service instances associated with
   a remote endpoint when a reboot is detected.

   **Rationale**: Services from rebooted ECU are no longer valid.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Update Stored Session ID
   :id: REQ_SD_056
   :satisfies: feat_req_someipsd_795
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify stored Session ID is updated after processing.

   The software shall update the stored Session ID to the new value
   after successfully processing an SD message.

   **Rationale**: Enables detection of subsequent reboots.

   **Code Location**: ``src/sd/sd_server.cpp``

Reboot Detection Error Handling
-------------------------------

.. requirement:: Error - Session ID Wrap-Around Handling
   :id: REQ_SD_052_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify wrap-around from 0xFFFF to 0x0001 is not detected as reboot.

   The software shall correctly handle Session ID wrap-around (0xFFFF to
   0x0001) and not falsely detect it as a reboot.

   **Rationale**: Normal Session ID progression should not trigger reboot.

   **Error Handling**: Use wrap-around detection threshold.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Error - First Session ID Storage
   :id: REQ_SD_050_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify first Session ID from new endpoint is stored without reboot detection.

   The software shall store the Session ID from a new endpoint without
   triggering reboot detection.

   **Rationale**: First contact cannot be a reboot.

   **Error Handling**: Store Session ID and skip reboot check.

   **Code Location**: ``src/sd/sd_server.cpp``

Options Parsing
===============

Options Array
-------------

.. requirement:: Parse Options Length Field
   :id: REQ_SD_060
   :satisfies: feat_req_someipsd_1096
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Options Length field is parsed from after Entries Array.

   The software shall parse the Options Length field as a 4-byte Big Endian
   value from the position immediately after the Entries Array.

   **Rationale**: Options Length indicates the size of the options array.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Option Type
   :id: REQ_SD_061
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Option Type is correctly parsed from first byte of option.

   The software shall parse the Option Type from byte 2 of each SD option
   (after length field).

   **Rationale**: Option Type determines the option format.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Option Length
   :id: REQ_SD_062
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Option Length is parsed from bytes 0-1 of option.

   The software shall parse the Option Length from bytes 0-1 of each SD
   option in Big Endian byte order.

   **Rationale**: Option Length indicates the option data size.

   **Code Location**: ``src/sd/sd_message.cpp``

IPv4 Endpoint Option
--------------------

.. requirement:: Parse IPv4 Endpoint Option Type
   :id: REQ_SD_063
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Option Type 0x04 is parsed as IPv4 Endpoint.

   The software shall recognize Option Type 0x04 as an IPv4 Endpoint
   option.

   **Rationale**: IPv4 Endpoint provides service endpoint information.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract IPv4 Address
   :id: REQ_SD_064
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify IPv4 address is extracted from bytes 4-7 of option.

   The software shall extract the IPv4 address from bytes 4-7 of the
   IPv4 Endpoint option.

   **Rationale**: IP address is needed for service communication.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract IPv4 Port Number
   :id: REQ_SD_065
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify port number is extracted from bytes 9-10 of IPv4 option.

   The software shall extract the port number from bytes 9-10 of the
   IPv4 Endpoint option in Big Endian byte order.

   **Rationale**: Port number is needed for service communication.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract IPv4 Protocol
   :id: REQ_SD_066
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify protocol is extracted from byte 8 of IPv4 option.

   The software shall extract the L4 protocol from byte 8 of the IPv4
   Endpoint option (UDP=0x11, TCP=0x06).

   **Rationale**: Protocol type determines transport layer.

   **Code Location**: ``src/sd/sd_message.cpp``

IPv6 Endpoint Option
--------------------

.. requirement:: Parse IPv6 Endpoint Option Type
   :id: REQ_SD_067
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Option Type 0x06 is parsed as IPv6 Endpoint.

   The software shall recognize Option Type 0x06 as an IPv6 Endpoint
   option.

   **Rationale**: IPv6 Endpoint provides service endpoint information.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract IPv6 Address
   :id: REQ_SD_068
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify IPv6 address is extracted from bytes 4-19 of option.

   The software shall extract the IPv6 address from bytes 4-19 of the
   IPv6 Endpoint option (16 bytes).

   **Rationale**: IPv6 address is needed for service communication.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract IPv6 Port Number
   :id: REQ_SD_069
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify port number is extracted from bytes 21-22 of IPv6 option.

   The software shall extract the port number from bytes 21-22 of the
   IPv6 Endpoint option in Big Endian byte order.

   **Rationale**: Port number is needed for service communication.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Extract IPv6 Protocol
   :id: REQ_SD_070
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify protocol is extracted from byte 20 of IPv6 option.

   The software shall extract the L4 protocol from byte 20 of the IPv6
   Endpoint option (UDP=0x11, TCP=0x06).

   **Rationale**: Protocol type determines transport layer.

   **Code Location**: ``src/sd/sd_message.cpp``

Configuration Option
--------------------

.. requirement:: Parse Configuration Option Type
   :id: REQ_SD_071
   :satisfies: feat_req_someipsd_1163
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify Option Type 0x01 is parsed as Configuration option.

   The software shall recognize Option Type 0x01 as a Configuration
   option.

   **Rationale**: Configuration option provides additional service parameters.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Configuration String
   :id: REQ_SD_072
   :satisfies: feat_req_someipsd_1163
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify configuration string is extracted from Configuration option.

   The software shall extract the configuration string from the
   Configuration option payload.

   **Rationale**: Configuration data for service parameters.

   **Code Location**: ``src/sd/sd_message.cpp``

Multicast Endpoint Options
--------------------------

.. requirement:: Parse IPv4 Multicast Option Type
   :id: REQ_SD_073
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Option Type 0x14 is parsed as IPv4 Multicast.

   The software shall recognize Option Type 0x14 as an IPv4 Multicast
   Endpoint option.

   **Rationale**: Multicast endpoint for event distribution.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse IPv6 Multicast Option Type
   :id: REQ_SD_074
   :satisfies: feat_req_someipsd_1112
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Option Type 0x16 is parsed as IPv6 Multicast.

   The software shall recognize Option Type 0x16 as an IPv6 Multicast
   Endpoint option.

   **Rationale**: Multicast endpoint for event distribution.

   **Code Location**: ``src/sd/sd_message.cpp``

Option Index References
-----------------------

.. requirement:: Parse First Option Index
   :id: REQ_SD_075
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify First Option Index is parsed from entry.

   The software shall parse the First Option Index (1st options run)
   from byte 1 of each SD entry.

   **Rationale**: Links entry to its endpoint options.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Second Option Index
   :id: REQ_SD_076
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Second Option Index is parsed from entry.

   The software shall parse the Second Option Index (2nd options run)
   from byte 2 of each SD entry.

   **Rationale**: Links entry to additional endpoint options.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Parse Option Counts
   :id: REQ_SD_077
   :satisfies: feat_req_someipsd_625
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify option counts are parsed from byte 3 of entry.

   The software shall parse the option counts (number of options in each
   run) from byte 3 of each SD entry.

   **Rationale**: Determines how many options to read for each run.

   **Code Location**: ``src/sd/sd_message.cpp``

Options Error Handling
----------------------

.. requirement:: Error - Invalid Option Type
   :id: REQ_SD_061_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify unknown option types are skipped with warning.

   The software shall skip options with unknown Option Type values and
   log a warning, continuing to process remaining options.

   **Rationale**: Forward compatibility with new option types.

   **Error Handling**: Log warning and skip option.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Truncated Option
   :id: REQ_SD_062_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when option extends beyond Options Length.

   The software shall return an error when an option would extend beyond
   the declared Options Length boundary.

   **Rationale**: Prevents buffer overread.

   **Error Handling**: Return MALFORMED_MESSAGE error.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Invalid Options Length
   :id: REQ_SD_060_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when Options Length exceeds remaining payload.

   The software shall return an error when the Options Length field
   exceeds the remaining SD payload size.

   **Rationale**: Prevents buffer overread.

   **Error Handling**: Return MALFORMED_MESSAGE error.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Invalid IP Address
   :id: REQ_SD_064_E01
   :status: implemented
   :priority: medium
   :category: error_path
   :verification: Unit test: Verify warning for invalid IP addresses (0.0.0.0, broadcast).

   The software shall log a warning when an endpoint option contains
   an invalid IP address (0.0.0.0 or broadcast address).

   **Rationale**: Invalid addresses may indicate configuration errors.

   **Error Handling**: Log warning but process option.

   **Code Location**: ``src/sd/sd_message.cpp``

.. requirement:: Error - Option Index Out of Range
   :id: REQ_SD_075_E01
   :status: implemented
   :priority: high
   :category: error_path
   :verification: Unit test: Verify error when option index exceeds available options.

   The software shall return an error when an entry's option index
   refers to a non-existent option.

   **Rationale**: Invalid option reference detection.

   **Error Handling**: Return MALFORMED_MESSAGE error.

   **Code Location**: ``src/sd/sd_message.cpp``

Service State Machine
=====================

.. requirement:: Service Find State
   :id: REQ_SD_080
   :satisfies: feat_req_someipsd_632
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify service enters Find state on client request.

   The software shall transition a service to the Find state when an
   application requests to find a service.

   **Rationale**: State machine for service discovery.

   **Code Location**: ``src/sd/sd_client.cpp``

.. requirement:: Send Find Service Message
   :id: REQ_SD_081
   :satisfies: feat_req_someipsd_632
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Find Service message is sent in Find state.

   The software shall generate and send a Find Service (Type 0) entry
   when in the Find state.

   **Rationale**: Initiates service discovery.

   **Code Location**: ``src/sd/sd_client.cpp``

.. requirement:: Service Available State
   :id: REQ_SD_082
   :satisfies: feat_req_someipsd_632
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify service enters Available state on offer received.

   The software shall transition a service to the Available state when
   a matching Offer Service entry is received.

   **Rationale**: Service has been discovered.

   **Code Location**: ``src/sd/sd_client.cpp``

.. requirement:: Offer Service Generation
   :id: REQ_SD_083
   :satisfies: feat_req_someipsd_633
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Offer Service message is generated when service goes up.

   The software shall generate and send an Offer Service (Type 1) entry
   when a local service becomes available.

   **Rationale**: Announces service availability.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Stop Offer Generation
   :id: REQ_SD_084
   :satisfies: feat_req_someipsd_634
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Stop Offer message is generated when service goes down.

   The software shall generate and send an Offer Service entry with
   TTL=0 when a local service becomes unavailable.

   **Rationale**: Announces service unavailability.

   **Code Location**: ``src/sd/sd_server.cpp``

Subscription Management
=======================

.. requirement:: Subscribe Eventgroup Request
   :id: REQ_SD_090
   :satisfies: feat_req_someipsd_629
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Subscribe Eventgroup entry is generated on subscription request.

   The software shall generate and send a Subscribe Eventgroup (Type 6)
   entry when an application requests to subscribe to an eventgroup.

   **Rationale**: Initiates event subscription.

   **Code Location**: ``src/sd/sd_client.cpp``

.. requirement:: Subscribe Eventgroup Acknowledgment
   :id: REQ_SD_091
   :satisfies: feat_req_someipsd_630
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Subscribe Eventgroup Ack entry is generated on subscription acceptance.

   The software shall generate and send a Subscribe Eventgroup Ack (Type 7)
   entry when a subscription request is accepted.

   **Rationale**: Confirms subscription to client.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Subscribe Eventgroup Negative Acknowledgment
   :id: REQ_SD_092
   :satisfies: feat_req_someipsd_630
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Subscribe Eventgroup Nack entry is generated on subscription rejection.

   The software shall generate and send a Subscribe Eventgroup Ack entry
   with TTL=0 when a subscription request is rejected.

   **Rationale**: Rejects subscription to client.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Subscription Renewal
   :id: REQ_SD_093
   :satisfies: feat_req_someipsd_748
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify subscription is renewed before TTL expiry.

   The software shall automatically renew subscriptions by sending a new
   Subscribe Eventgroup entry before the subscription TTL expires.

   **Rationale**: Maintains continuous event subscription.

   **Code Location**: ``src/sd/sd_client.cpp``

.. requirement:: Stop Subscribe Generation
   :id: REQ_SD_094
   :satisfies: feat_req_someipsd_629
   :status: implemented
   :priority: high
   :category: happy_path
   :verification: Unit test: Verify Stop Subscribe entry is generated on unsubscribe request.

   The software shall generate and send a Subscribe Eventgroup entry
   with TTL=0 when an application requests to unsubscribe.

   **Rationale**: Terminates event subscription.

   **Code Location**: ``src/sd/sd_client.cpp``

Timing and Repetition
=====================

.. requirement:: Initial Offer Delay
   :id: REQ_SD_100
   :satisfies: feat_req_someipsd_425
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify initial delay before first offer.

   The software shall wait for a configurable initial delay before
   sending the first Offer Service message after startup.

   **Rationale**: Allows network to stabilize.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Offer Repetition
   :id: REQ_SD_101
   :satisfies: feat_req_someipsd_425
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify offer is repeated with increasing intervals.

   The software shall repeat Offer Service messages with exponentially
   increasing intervals during the repetition phase.

   **Rationale**: Ensures discovery while reducing network traffic.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Cyclic Offer
   :id: REQ_SD_102
   :satisfies: feat_req_someipsd_425
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify cyclic offer at configured interval.

   The software shall send Offer Service messages at a configurable
   cyclic interval during the main phase.

   **Rationale**: Periodic refresh of service availability.

   **Code Location**: ``src/sd/sd_server.cpp``

.. requirement:: Find Repetition
   :id: REQ_SD_103
   :satisfies: feat_req_someipsd_632
   :status: implemented
   :priority: medium
   :category: happy_path
   :verification: Unit test: Verify find is repeated until service is found or timeout.

   The software shall repeat Find Service messages with increasing
   intervals until the service is found or maximum attempts reached.

   **Rationale**: Persistent service discovery.

   **Code Location**: ``src/sd/sd_client.cpp``

Traceability
============

Implementation Files
--------------------

* ``include/sd/sd_types.h`` - SD type definitions
* ``include/sd/sd_message.h`` - SD message interface
* ``include/sd/sd_client.h`` - SD client interface
* ``include/sd/sd_server.h`` - SD server interface
* ``src/sd/sd_message.cpp`` - SD message implementation
* ``src/sd/sd_client.cpp`` - SD client implementation
* ``src/sd/sd_server.cpp`` - SD server implementation

Test Files
----------

* ``tests/test_sd.cpp`` - SD unit tests
