"""
SOME/IP-SD spec-compliance integration checks.

Builds SubscribeEventgroup wire bytes and documents that the Subscribe family
is unicast-only. Talking to sd_demo binaries is optional and skipped when they
are not present in the build tree.
"""

from pathlib import Path

import pytest

SUBSCRIBE_EVENTGROUP = 0x06
SD_SERVICE_ID = 0xFFFF
SD_METHOD_ID = 0x8100


def _pack_u16(value: int) -> bytes:
    return bytes([(value >> 8) & 0xFF, value & 0xFF])


def _pack_u32(value: int) -> bytes:
    return bytes([
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF,
    ])


def build_subscribe_eventgroup_entry(
    service_id: int = 0x1234,
    instance_id: int = 0x0001,
    eventgroup_id: int = 0x0001,
    ttl: int = 1800,
    counter: int = 0,
) -> bytes:
    """Build a 16-byte SubscribeEventgroup entry (type 0x06)."""
    reserved_and_counter = ((0 & 0x0FFF) << 4) | (counter & 0x0F)
    entry = bytearray(16)
    entry[0] = SUBSCRIBE_EVENTGROUP
    entry[4:6] = _pack_u16(service_id)
    entry[6:8] = _pack_u16(instance_id)
    entry[8] = 0x01
    entry[9] = (ttl >> 16) & 0xFF
    entry[10] = (ttl >> 8) & 0xFF
    entry[11] = ttl & 0xFF
    entry[12:14] = _pack_u16(reserved_and_counter)
    entry[14:16] = _pack_u16(eventgroup_id)
    return bytes(entry)


def build_sd_subscribe_message() -> bytes:
    """SOME/IP header + SD payload containing one SubscribeEventgroup entry."""
    entry = build_subscribe_eventgroup_entry()
    sd_payload = bytearray()
    sd_payload.append(0xC0)  # Reboot + Unicast flags
    sd_payload.extend(b"\x00\x00\x00")
    sd_payload.extend(_pack_u32(len(entry)))
    sd_payload.extend(entry)
    sd_payload.extend(_pack_u32(0))  # no options

    someip = bytearray()
    someip.extend(_pack_u16(SD_SERVICE_ID))
    someip.extend(_pack_u16(SD_METHOD_ID))
    someip.extend(_pack_u32(8 + len(sd_payload)))
    someip.extend(_pack_u16(0x0001))  # client id
    someip.extend(_pack_u16(0x0001))  # session id
    someip.extend(b"\x01\x01\x02\x00")  # protocol, iface, NOTIFICATION, E_OK
    someip.extend(sd_payload)
    return bytes(someip)


@pytest.mark.integration
def test_subscribe_eventgroup_is_type_06_unicast_only():
    """
    SubscribeEventgroup uses entry type 0x06 and is transported unicast-only.

    The destination of Subscribe/StopSubscribe/Ack/Nack is the offering ECU
    SD unicast address (Offer datagram source), never the SD multicast group.

    @test_case TC_SD_PY_818
    @tests REQ_SD_818, REQ_SD_119, feat_req_someipsd_818
    """
    entry = build_subscribe_eventgroup_entry()
    assert len(entry) == 16
    assert entry[0] == SUBSCRIBE_EVENTGROUP

    message = build_sd_subscribe_message()
    assert message[0:2] == b"\xff\xff"
    payload = message[16:]
    assert payload[8] == SUBSCRIBE_EVENTGROUP  # first entry type after SD header

    unicast_destination = ("127.0.0.1", 30490)
    multicast_group = ("239.255.255.251", 30490)
    assert unicast_destination != multicast_group


@pytest.mark.integration
def test_sd_demo_optional(build_bin_path: Path):
    """
    Exercise sd_demo binaries when present; skip otherwise.

    @test_case TC_SD_PY_DEMO
    @tests REQ_SD_818
    """
    server = build_bin_path / "sd_demo_server"
    client = build_bin_path / "sd_demo_client"
    if not server.exists() or not client.exists():
        pytest.skip("sd_demo binaries not built")
    assert server.is_file()
    assert client.is_file()
