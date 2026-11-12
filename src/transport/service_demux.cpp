/********************************************************************************
 * Copyright (c) 2026 Vinicius Tadeu Zein
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "transport/service_demux.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/result.h"
#include "platform/message_ptr.h"
#include "platform/thread.h"
#include "transport/endpoint.h"
#include "transport/message_rejection.h"
#include "transport/transport.h"

namespace someip::transport {

ServiceDemux::FlightGuard::FlightGuard(ServiceDemux& demux, std::size_t index)
    : demux_(&demux), index_(index)
{
}

ServiceDemux::FlightGuard::~FlightGuard()
{
    demux_->release_flight(index_);
}

ServiceDemux::ServiceDemux(ITransport& transport) : transport_(transport)
{
}

ServiceDemux::~ServiceDemux()
{
#ifdef __cpp_exceptions
    try {
        static_cast<void>(stop());
    } catch (...) {}  // NOLINT(bugprone-empty-catch) destructor must not throw
#else
    static_cast<void>(stop());
#endif
}

bool ServiceDemux::slot_is_free(const Slot& slot)
{
    return !slot.occupied && !slot.closing && slot.in_flight == 0U;
}

Result ServiceDemux::register_service(uint16_t service_id, ITransportListener& handler)
{
    platform::ScopedLock const lock(table_mutex_);
    for (const Slot& slot : slots_) {
        if (slot.occupied && slot.service_id == service_id) {
            return Result::INVALID_STATE;
        }
    }

    for (Slot& slot : slots_) {
        if (!slot_is_free(slot)) {
            continue;
        }
        slot.service_id = service_id;
        slot.handler = &handler;
        slot.occupied = true;
        slot.closing = false;
        slot.in_flight = 0U;
        return Result::SUCCESS;
    }
    return Result::RESOURCE_EXHAUSTED;
}

Result ServiceDemux::unregister_service(uint16_t service_id)
{
    platform::ScopedLock const lock(table_mutex_);
    Slot* selected = nullptr;
    for (Slot& slot : slots_) {
        if (slot.occupied && slot.service_id == service_id) {
            selected = &slot;
            break;
        }
    }
    if (selected == nullptr) {
        return Result::SERVICE_NOT_FOUND;
    }

    selected->occupied = false;
    selected->closing = true;
    while (selected->in_flight > 0U) {
        // The callback for this service id is on the transport thread.
        // Waiting here from that callback deadlocks; callers must not do so.
        drained_.wait(table_mutex_, [&selected] { return selected->in_flight == 0U; });
    }
    selected->handler = nullptr;
    selected->service_id = 0U;
    selected->closing = false;
    return Result::SUCCESS;
}

bool ServiceDemux::contains(uint16_t service_id) const
{
    platform::ScopedLock const lock(table_mutex_);
    for (const Slot& slot : slots_) {
        if (slot.occupied && slot.service_id == service_id) {
            return true;
        }
    }
    return false;
}

Result ServiceDemux::start()
{
    platform::ScopedLock const lock(lifecycle_mutex_);
    if (started_) {
        return Result::SUCCESS;
    }
    if (transport_.is_running()) {
        return Result::INVALID_STATE;
    }

    transport_.set_listener(this);
    const Result started = transport_.start();
    if (started != Result::SUCCESS) {
        transport_.set_listener(nullptr);
        return started;
    }
    started_ = true;
    return Result::SUCCESS;
}

Result ServiceDemux::stop()
{
    platform::ScopedLock const lock(lifecycle_mutex_);
    if (!started_) {
        return Result::SUCCESS;
    }

    const Result stopped = transport_.stop();
    if (!transport_.is_running()) {
        started_ = false;
    }
    return stopped;
}

ServiceDemux::Slot& ServiceDemux::slot_at(std::size_t index)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return slots_[index];
}

ServiceDemux::BroadcastTarget& ServiceDemux::target_at(
    std::array<BroadcastTarget, MAX_DEMUX_SERVICES>& targets, std::size_t index)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return targets[index];
}

bool ServiceDemux::claim(uint16_t service_id, Claimed& claimed)
{
    platform::ScopedLock const lock(table_mutex_);
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        Slot& slot = slot_at(index);
        if (!slot.occupied || slot.service_id != service_id || slot.handler == nullptr) {
            continue;
        }
        ++slot.in_flight;
        claimed.handler = slot.handler;
        claimed.index = index;
        return true;
    }
    return false;
}

bool ServiceDemux::claim_at(std::size_t index, uint16_t service_id, Claimed& claimed)
{
    platform::ScopedLock const lock(table_mutex_);
    if (index >= slots_.size()) {
        return false;
    }
    Slot& slot = slot_at(index);
    if (!slot.occupied || slot.service_id != service_id || slot.handler == nullptr) {
        return false;
    }
    ++slot.in_flight;
    claimed.handler = slot.handler;
    claimed.index = index;
    return true;
}

std::size_t ServiceDemux::snapshot_targets(std::array<BroadcastTarget, MAX_DEMUX_SERVICES>& targets)
{
    platform::ScopedLock const lock(table_mutex_);
    std::size_t count = 0;
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        const Slot& slot = slot_at(index);
        if (!slot.occupied || slot.handler == nullptr) {
            continue;
        }
        BroadcastTarget& target = target_at(targets, count);
        target.index = index;
        target.service_id = slot.service_id;
        ++count;
    }
    return count;
}

template <typename Callback>
void ServiceDemux::broadcast(Callback&& callback)
{
    // Record who is registered, then claim one handler as its callback starts.
    // A callback can unregister a later service before that service is claimed.
    std::array<BroadcastTarget, MAX_DEMUX_SERVICES> targets{};
    const std::size_t count = snapshot_targets(targets);
    for (std::size_t index = 0; index < count; ++index) {
        const BroadcastTarget& target = target_at(targets, index);
        Claimed claimed;
        if (!claim_at(target.index, target.service_id, claimed) || claimed.handler == nullptr) {
            continue;
        }
        FlightGuard const guard(*this, claimed.index);
        callback(*claimed.handler);
    }
}

void ServiceDemux::release_flight(std::size_t index)
{
    platform::ScopedLock const lock(table_mutex_);
    if (index >= slots_.size() || slot_at(index).in_flight == 0U) {
        return;
    }
    --slot_at(index).in_flight;
    drained_.notify_all();
}

void ServiceDemux::on_message_received(MessagePtr message, const Endpoint& sender)
{
    on_message_received(message, sender, Endpoint());
}

void ServiceDemux::on_message_received(const MessagePtr& message, const Endpoint& sender,
                                       const Endpoint& destination)
{
    if (!message) {
        return;
    }

    Claimed claimed;
    if (!claim(message->get_service_id(), claimed)) {
        return;
    }

    FlightGuard const guard(*this, claimed.index);
    claimed.handler->on_message_received(message, sender, destination);
}

void ServiceDemux::on_connection_lost(const Endpoint& endpoint)
{
    broadcast([&endpoint](ITransportListener& handler) { handler.on_connection_lost(endpoint); });
}

void ServiceDemux::on_connection_established(const Endpoint& endpoint)
{
    broadcast(
        [&endpoint](ITransportListener& handler) { handler.on_connection_established(endpoint); });
}

void ServiceDemux::on_error(Result error)
{
    broadcast([error](ITransportListener& handler) { handler.on_error(error); });
}

void ServiceDemux::on_message_rejected(const MessageRejectionInfo& info)
{
    if (!info.has_message_id) {
        return;
    }

    Claimed claimed;
    if (!claim(info.message_id.service_id, claimed)) {
        return;
    }

    FlightGuard const guard(*this, claimed.index);
    claimed.handler->on_message_rejected(info);
}

}  // namespace someip::transport
