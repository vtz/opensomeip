---
title: SOME/IP Events API
description: C++ API for SOME/IP publish/subscribe events. Event groups, notification handlers, and subscriber management for automotive SOA.
---

# Events Layer

The Events layer implements SOME/IP event notifications and field publications, enabling real-time data distribution between services and clients using publish/subscribe patterns.

## Architecture

### Components

#### EventPublisher
- **Purpose**: Service-side event and field publication
- **Features**:
  - Event registration with configurable policies
  - Automatic periodic event publishing
  - Field value publishing (on-demand or on-change)
  - Subscription management for event groups
  - Cyclic and triggered publication modes

#### EventSubscriber
- **Purpose**: Client-side event reception and field requests
- **Features**:
  - Event group subscription with filters
  - Event notification callbacks
  - Field value requests (polling)
  - Subscription status tracking
  - Automatic reconnection handling

#### EventTypes
- **Purpose**: Event system types and configurations
- **Includes**:
  - Event and notification types
  - Subscription states and policies
  - Event filters and configurations
  - Publication policies and reliability options

## Usage Examples

### Publisher Side - Publishing Events

```cpp
#include <events/event_publisher.h>

using namespace someip::events;

// Create publisher for sensor service
EventPublisher publisher(0x1001, 0x0001);  // Service ID, Instance ID
publisher.initialize();

// Register temperature event (periodic)
EventConfig temp_config;
temp_config.event_id = 0x8001;
temp_config.eventgroup_id = 0x0001;
temp_config.notification_type = NotificationType::PERIODIC;
temp_config.cycle_time = std::chrono::milliseconds(1000);
temp_config.reliability = Reliability::UNRELIABLE;

publisher.register_event(temp_config);

// Register speed field (on change)
EventConfig speed_config;
speed_config.event_id = 0x8002;
speed_config.eventgroup_id = 0x0001;
speed_config.notification_type = NotificationType::ON_CHANGE;
speed_config.is_field = true;

publisher.register_event(speed_config);

// Publish events (automatic for periodic, manual for on-change)
std::vector<uint8_t> speed_data = {0x00, 0x50};  // 80 km/h
publisher.publish_field(0x8002, speed_data);

// Handle subscriptions
publisher.handle_subscription(0x0001, client_id);  // Event group 1
```

### Subscriber Side - Receiving Events

```cpp
#include <events/event_subscriber.h>

using namespace someip::events;

// Create subscriber
EventSubscriber subscriber(0xABCD);  // Client ID
subscriber.initialize();

// Subscribe to event group
subscriber.subscribe_eventgroup(0x1001, 0x0001, 0x0001,
    [](const EventNotification& notification) {
        // Handle event notification
        std::cout << "Received event 0x" << std::hex << notification.event_id << std::endl;
    },
    [](uint16_t event_id, SubscriptionState state) {
        // Handle subscription status changes
        std::cout << "Subscription state: " << (int)state << std::endl;
    });

// Request field value
subscriber.request_field(0x1001, 0x0001, 0x8002,
    [](const EventNotification& field_value) {
        // Handle field response
    });
```

## Event Types and Policies

### Notification Types

- **PERIODIC**: Regular time-based notifications
- **ON_CHANGE**: Notifications when value changes
- **ON_CHANGE_WITH_FILTER**: Filtered change notifications
- **POLLING**: Client-requested updates only

### Reliability Types

- **UNKNOWN**: Default/unconfigured
- **UNRELIABLE**: UDP-based, best-effort delivery
- **RELIABLE**: TCP-based, guaranteed delivery

### Publication Policies

- **IMMEDIATE**: Publish immediately on change
- **CYCLIC**: Publish at regular intervals
- **ON_REQUEST**: Publish only when requested
- **TRIGGERED**: Publish when externally triggered

## Event Groups

Event groups allow logical grouping of related events:

```cpp
// Publisher side
EventConfig event1, event2;
event1.eventgroup_id = event2.eventgroup_id = 0x0001;  // Same group

// Subscriber side
subscriber.subscribe_eventgroup(service_id, instance_id, 0x0001,
                               notification_callback);
```

## Fields vs Events

### Fields
- **Purpose**: Represent current state/values
- **Publishing**: Immediate or on-change
- **Access**: Can be polled individually
- **Example**: Current vehicle speed, temperature

### Events
- **Purpose**: Notify about changes or occurrences
- **Publishing**: Periodic or triggered
- **Access**: Subscription-based only
- **Example**: Speed changes, temperature alerts

## Filters and Selectors

Event filters allow selective notifications:

```cpp
std::vector<EventFilter> filters = {
    {0x8001, {0x01}},  // Only temperature > threshold
    {0x8002, {0x00}}   // Only speed changes
};

subscriber.subscribe_eventgroup(service_id, instance_id, eventgroup_id,
                               callback, status_callback, filters);
```

## Subscription Management

### States
- **REQUESTED**: Initial subscription request
- **SUBSCRIBED**: Active subscription confirmed
- **PENDING**: Awaiting confirmation
- **REJECTED**: Subscription denied
- **EXPIRED**: Subscription timed out

### Lifecycle
```
Client Request → PENDING → SUBSCRIBED → Active Notifications
     ↓              ↓           ↓
  REJECTED       EXPIRED     Unsubscribe
```

## Performance Considerations

### Publisher Optimizations
- **Cyclic Publishing**: Batched periodic updates
- **Change Detection**: Only publish on actual changes
- **Client Management**: Efficient subscription tracking
- **Thread Safety**: Concurrent subscription handling

### Subscriber Optimizations
- **Callback Efficiency**: Fast notification processing
- **Filter Application**: Server-side filtering when possible
- **Connection Management**: Automatic reconnection
- **Resource Limits**: Configurable subscription limits

## Safety Considerations (non-certified)
- **Timeout Management**: Configurable publication timeouts
- **Error Handling**: Graceful degradation on failures
- **State Tracking**: Reliable subscription state management
- **Thread Safety**: Thread-safe operations

### Deterministic Behavior
- **Publication Timing**: Predictable cyclic publishing
- **Notification Ordering**: Consistent event sequencing
- **Subscription Confirmation**: Reliable state transitions

## Integration with RPC

Events complement RPC for complete SOME/IP services:

```cpp
// RPC for commands and responses
rpc_client.call_method_sync(0x1001, 0x0001, command_data);

// Events for real-time data
event_subscriber.subscribe_eventgroup(0x1001, 0x0001, 0x0001, event_callback);
```

## Testing

Unit tests cover:
- Event configuration and registration
- Subscription state management
- Notification publishing and reception
- Filter application and validation
- Thread safety and error conditions

See `test_events.cpp` for comprehensive test coverage.

## Dependencies

- **someip-core**: Basic message structures
- **someip-transport**: Network communication
- **someip-rpc**: Subscription request handling

## Future Extensions

- **Advanced Filtering**: Complex filter expressions
- **Quality of Service**: Priority-based event delivery
- **Event History**: Buffered event replay
- **Security**: Event authentication and encryption
