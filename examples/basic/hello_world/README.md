# Hello World Example

This is the simplest SOME/IP example demonstrating basic client-server message exchange.

## Overview

The Hello World example consists of two programs:
- **Server**: Listens for incoming messages and responds with a greeting
- **Client**: Sends a "Hello" message and waits for the server's response

## Files

- `server.cpp` - The Hello World server implementation
- `client.cpp` - The Hello World client implementation
- `README.md` - This documentation

## Building the Example

First, build the entire project from the main directory:

```bash
# From the project root directory
mkdir build && cd build
cmake ..
make
```

## Running the Example

### Terminal 1 - Start the Server
```bash
# From the project root directory (after building)
./build/bin/hello_world_server
```

You should see:
```
=== SOME/IP Hello World Server ===
Press Ctrl+C to exit

Hello World Server started on 127.0.0.1:30501
Waiting for 'Hello' messages...
```

### Terminal 2 - Run the Client
```bash
# From the project root directory (after building)
./build/bin/hello_world_client
```

You should see:
```
=== SOME/IP Hello World Client ===
Hello World Client started on 127.0.0.1:xxxxx
Sending message: 'Hello from Client!' to 127.0.0.1:30501
Received message from 127.0.0.1:30501
Message: Service:0x1000, Method:0x0001, Type:RESPONSE, Length:52
Server responded: 'Hello World! Server received: Hello from Client!'
Client finished.
```

And the server will show:
```
Received message from 127.0.0.1:xxxxx
Message: Service:0x1000, Method:0x0001, Type:REQUEST, Length:20
Client said: 'Hello from Client!'
Sent greeting: 'Hello World! Server received: Hello from Client!'
```

You should see:
```
=== SOME/IP Hello World Client ===
Hello World Client started on 127.0.0.1:xxxxx
Sending message: 'Hello from Client!' to 127.0.0.1:30501
Received message from 127.0.0.1:30501
Message: Service:0x1000, Method:0x0001, Type:RESPONSE, Length:52
Server responded: 'Hello World! Server received: Hello from Client!'
Client finished.
```

And the server will show:
```
Received message from 127.0.0.1:xxxxx
Message: Service:0x1000, Method:0x0001, Type:REQUEST, Length:20
Client said: 'Hello from Client!'
Sent greeting: 'Hello World! Server received: Hello from Client!'
```

## What This Example Demonstrates

1. **Basic SOME/IP Message Creation**: Creating request and response messages with proper headers
2. **UDP Transport**: Using UDP for message transport between client and server
3. **Message Exchange**: Simple request-response pattern
4. **Payload Handling**: Converting strings to/from message payloads
5. **Transport Listeners**: Implementing event-driven message reception

## Code Structure

### Server
- Implements `ITransportListener` to receive messages
- Checks for valid service/method ID combinations
- Creates and sends response messages

### Client
- Implements `ITransportListener` to receive responses
- Creates request messages with payload
- Uses condition variables to wait for responses

## Next Steps

After understanding this basic example, try:
- [Method Calls Example](../method_calls/) - Learn about RPC patterns
- [Events Example](../events/) - Learn about publish-subscribe patterns
