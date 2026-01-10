#!/usr/bin/env python3
"""
Multicast listener for infrastructure validation.
Joins the SOME/IP-SD multicast group and prints received messages.
"""
import socket
import struct
import sys

MCAST_GROUP = "224.224.224.245"
MCAST_PORT = 30490

def main():
    print(f"=== Multicast Listener ===")
    print(f"Joining group: {MCAST_GROUP}:{MCAST_PORT}")
    print()

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # Bind to the multicast port
    sock.bind(('', MCAST_PORT))

    # Join multicast group
    mreq = struct.pack("4sl", socket.inet_aton(MCAST_GROUP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    print("Listening... (Ctrl+C to stop)")
    print()

    try:
        while True:
            data, addr = sock.recvfrom(1024)
            print(f"Received from {addr[0]}:{addr[1]}")
            print(f"  Data ({len(data)} bytes): {data[:64]}{'...' if len(data) > 64 else ''}")
            print(f"  Hex: {data[:32].hex()}")
            print()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
