# Infrastructure Validation Tests

Test multicast connectivity between host and Docker containers before running vsomeip interop tests.

## Test 1: Host-to-Host Multicast (Sanity Check)

Verify multicast works locally on your Mac:

```bash
# Terminal 1: Start listener
python3 examples/infra_test/multicast_listener.py

# Terminal 2: Send message
python3 examples/infra_test/multicast_sender.py
```

**Expected**: Listener shows received message.

---

## Test 2: Container-to-Container Multicast

Both processes in Docker (same network):

```bash
# Terminal 1: Listener in container
docker run --rm -it --network host python:3.11-slim \
  python3 -c "
import socket, struct
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', 30490))
mreq = struct.pack('4sl', socket.inet_aton('224.224.224.245'), socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
print('Listening on 224.224.224.245:30490...')
while True:
    data, addr = sock.recvfrom(1024)
    print(f'Received from {addr}: {data}')
"

# Terminal 2: Sender in container
docker run --rm --network host python:3.11-slim \
  python3 -c "
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
sock.sendto(b'TEST-FROM-CONTAINER', ('224.224.224.245', 30490))
print('Sent!')
"
```

**Expected**: Listener shows received message.

---

## Test 3: Host → Container Multicast

Host sends, container receives:

```bash
# Terminal 1: Listener in container (--network host required for multicast)
docker run --rm -it --network host python:3.11-slim \
  python3 -c "
import socket, struct
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', 30490))
mreq = struct.pack('4sl', socket.inet_aton('224.224.224.245'), socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
print('Listening on 224.224.224.245:30490...')
while True:
    data, addr = sock.recvfrom(1024)
    print(f'Received from {addr}: {data}')
"

# Terminal 2: Sender on host
python3 examples/infra_test/multicast_sender.py
```

**Expected**: Container listener shows message from host.

---

## Test 4: Container → Host Multicast

Container sends, host receives:

```bash
# Terminal 1: Listener on host
python3 examples/infra_test/multicast_listener.py

# Terminal 2: Sender in container
docker run --rm --network host python:3.11-slim \
  python3 -c "
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
sock.sendto(b'TEST-FROM-CONTAINER', ('224.224.224.245', 30490))
print('Sent!')
"
```

**Expected**: Host listener shows message from container.

---

## Colima Limitations

⚠️ **`--network host` only works on Linux, not on macOS with Colima.**

For Colima, multicast between host and container is NOT directly supported. Options:

1. **Run both in containers**: Put your stack AND vsomeip in Docker containers on the same Docker network
2. **Use Linux VM**: Run tests inside the Colima VM
3. **Use native Linux**: Run on actual Linux machine

### Alternative: Both in Docker

```bash
# Create a network
docker network create someip-net

# Run listener container
docker run --rm -it --network someip-net --name listener python:3.11-slim \
  python3 -c "..." 

# Run sender container  
docker run --rm --network someip-net python:3.11-slim \
  python3 -c "..."
```

Note: Standard Docker bridge networks don't support multicast. You may need:
- `--network host` (Linux only)
- macvlan network
- Or run both apps in same container


