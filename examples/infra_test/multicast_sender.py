#!/usr/bin/env python3
"""
Multicast sender for infrastructure validation.
Sends a test message to the SOME/IP-SD multicast group.
"""
import socket
import sys
import time

MCAST_GROUP = "224.224.224.245"
MCAST_PORT = 30490

def main():
    print(f"=== Multicast Sender ===")
    print(f"Target: {MCAST_GROUP}:{MCAST_PORT}")
    print()

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

    # Set TTL for multicast
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)

    # Enable loopback (so we can receive our own messages if listener is on same host)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)

    message = b"SOMEIP-SD-TEST-" + str(int(time.time())).encode()

    print(f"Sending: {message}")
    sock.sendto(message, (MCAST_GROUP, MCAST_PORT))
    print("Sent!")

    sock.close()

if __name__ == "__main__":
    main()
