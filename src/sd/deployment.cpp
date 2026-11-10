/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
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

#include "sd/deployment.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "common/result.h"
#include "platform/containers.h"
#include "sd/sd_types.h"

namespace someip::sd {
namespace {

constexpr uint8_t UDP_PROTOCOL = 0x11;
constexpr uint8_t TCP_PROTOCOL = 0x06;
constexpr std::size_t MAX_TOKEN = 128;

struct Cursor {
    const char* cur{nullptr};
    const char* end{nullptr};
    Result error{Result::SUCCESS};

    bool ok() const
    {
        return error == Result::SUCCESS;
    }

    void fail(Result result)
    {
        if (error == Result::SUCCESS) {
            error = result;
        }
    }

    void skip()
    {
        while (cur < end) {
            const char ch = *cur;
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                break;
            }
            ++cur;
        }
    }

    bool take(char expected)
    {
        skip();
        if (cur < end && *cur == expected) {
            ++cur;
            return true;
        }
        return false;
    }
};

bool key_is(const char* key, const char* literal)
{
    return std::strcmp(key, literal) == 0;
}

bool parse_string(Cursor& cursor, std::array<char, MAX_TOKEN>& buffer)
{
    cursor.skip();
    if (cursor.cur >= cursor.end || *cursor.cur != '"') {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    ++cursor.cur;
    char* out = buffer.data();
    const char* const limit = out + (buffer.size() - 1);
    while (cursor.cur < cursor.end) {
        char ch = *cursor.cur;
        ++cursor.cur;
        if (ch == '"') {
            *out = '\0';
            return true;
        }
        if (static_cast<unsigned char>(ch) < 0x20U) {
            cursor.fail(Result::MALFORMED_MESSAGE);
            return false;
        }
        if (ch == '\\') {
            if (cursor.cur >= cursor.end) {
                cursor.fail(Result::MALFORMED_MESSAGE);
                return false;
            }
            const char escaped = *cursor.cur;
            ++cursor.cur;
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    ch = escaped;
                    break;
                case 'b':
                    ch = '\b';
                    break;
                case 'f':
                    ch = '\f';
                    break;
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                case 't':
                    ch = '\t';
                    break;
                default:
                    cursor.fail(Result::MALFORMED_MESSAGE);
                    return false;
            }
        }
        if (out >= limit) {
            cursor.fail(Result::BUFFER_OVERFLOW);
            return false;
        }
        *out = ch;
        ++out;
    }
    cursor.fail(Result::MALFORMED_MESSAGE);
    return false;
}

bool parse_u64_text(const char* text, uint64_t& value)
{
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    const char* cur = text;
    int base = 10;
    if (cur[0] == '0' && (cur[1] == 'x' || cur[1] == 'X')) {
        base = 16;
        cur += 2;
        if (*cur == '\0') {
            return false;
        }
    }
    else if (cur[0] == '0' && cur[1] != '\0') {
        return false;
    }
    uint64_t acc = 0;
    if (*cur == '\0') {
        return false;
    }
    while (*cur != '\0') {
        const char ch = *cur;
        int digit = -1;
        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        }
        else if (base == 16 && ch >= 'a' && ch <= 'f') {
            digit = ch - 'a' + 10;
        }
        else if (base == 16 && ch >= 'A' && ch <= 'F') {
            digit = ch - 'A' + 10;
        }
        if (digit < 0 || digit >= base) {
            return false;
        }
        if (acc > (UINT64_MAX - static_cast<uint64_t>(digit)) / static_cast<uint64_t>(base)) {
            return false;
        }
        acc = acc * static_cast<uint64_t>(base) + static_cast<uint64_t>(digit);
        ++cur;
    }
    value = acc;
    return true;
}

bool parse_u64(Cursor& cursor, uint64_t& value)
{
    cursor.skip();
    if (cursor.cur >= cursor.end) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    if (*cursor.cur == '"') {
        std::array<char, MAX_TOKEN> text{};
        if (!parse_string(cursor, text)) {
            return false;
        }
        if (!parse_u64_text(text.data(), value)) {
            cursor.fail(Result::MALFORMED_MESSAGE);
            return false;
        }
        return true;
    }
    if (*cursor.cur < '0' || *cursor.cur > '9') {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    if (*cursor.cur == '0' && (cursor.cur + 1) < cursor.end && cursor.cur[1] >= '0' &&
        cursor.cur[1] <= '9') {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    uint64_t acc = 0;
    while (cursor.cur < cursor.end && *cursor.cur >= '0' && *cursor.cur <= '9') {
        const auto digit = static_cast<uint64_t>(*cursor.cur - '0');
        if (acc > (UINT64_MAX - digit) / 10U) {
            cursor.fail(Result::INVALID_ARGUMENT);
            return false;
        }
        acc = acc * 10U + digit;
        ++cursor.cur;
    }
    value = acc;
    return true;
}

bool parse_bounded(Cursor& cursor, uint64_t limit, uint64_t& value)
{
    if (!parse_u64(cursor, value)) {
        return false;
    }
    if (value > limit) {
        cursor.fail(Result::INVALID_ARGUMENT);
        return false;
    }
    return true;
}

bool store_string(platform::String<>& dest, const char* text)
{
    const std::size_t length = std::strlen(text);
    if (length > dest.max_size()) {
        return false;
    }
    dest.assign(text, length);
    return true;
}

bool same_instance(const DeployedService& service, uint16_t service_id, uint16_t instance_id)
{
    return service.instance.service_id == service_id && service.instance.instance_id == instance_id;
}

bool parse_eventgroups(Cursor& cursor, platform::Vector<uint16_t>& eventgroups)
{
    if (!cursor.take('[')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    if (cursor.take(']')) {
        return true;
    }
    for (;;) {
        uint64_t value = 0;
        if (!parse_bounded(cursor, 0xFFFFU, value)) {
            return false;
        }
        if (eventgroups.size() >= eventgroups.max_size()) {
            cursor.fail(Result::BUFFER_OVERFLOW);
            return false;
        }
        eventgroups.push_back(static_cast<uint16_t>(value));
        if (!cursor.take(',')) {
            break;
        }
    }
    if (!cursor.take(']')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    return true;
}

bool parse_service_object(Cursor& cursor, DeployedService& service)
{
    if (!cursor.take('{')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    bool saw_service = false;
    bool saw_instance = false;
    bool saw_major = false;
    bool saw_minor = false;
    bool saw_unreliable = false;
    bool saw_reliable = false;
    bool saw_groups = false;
    if (cursor.take('}')) {
        cursor.fail(Result::INVALID_ARGUMENT);
        return false;
    }
    for (;;) {
        std::array<char, MAX_TOKEN> key{};
        if (!parse_string(cursor, key) || !cursor.take(':')) {
            if (cursor.ok()) {
                cursor.fail(Result::MALFORMED_MESSAGE);
            }
            return false;
        }
        uint64_t value = 0;
        if (key_is(key.data(), "service_id")) {
            if (saw_service || !parse_bounded(cursor, 0xFFFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            service.instance.service_id = static_cast<uint16_t>(value);
            saw_service = true;
        }
        else if (key_is(key.data(), "instance_id")) {
            if (saw_instance || !parse_bounded(cursor, 0xFFFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            service.instance.instance_id = static_cast<uint16_t>(value);
            saw_instance = true;
        }
        else if (key_is(key.data(), "major_version")) {
            if (saw_major || !parse_bounded(cursor, 0xFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            service.instance.major_version = static_cast<uint8_t>(value);
            saw_major = true;
        }
        else if (key_is(key.data(), "minor_version")) {
            if (saw_minor || !parse_bounded(cursor, 0xFFFFFFFFULL, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            service.instance.minor_version = static_cast<uint32_t>(value);
            saw_minor = true;
        }
        else if (key_is(key.data(), "unreliable_port")) {
            if (saw_unreliable || !parse_bounded(cursor, 0xFFFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            service.unreliable_port = static_cast<uint16_t>(value);
            saw_unreliable = true;
        }
        else if (key_is(key.data(), "reliable_port")) {
            if (saw_reliable || !parse_bounded(cursor, 0xFFFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            service.reliable_port = static_cast<uint16_t>(value);
            saw_reliable = true;
        }
        else if (key_is(key.data(), "eventgroups")) {
            if (saw_groups || !parse_eventgroups(cursor, service.eventgroups)) {
                if (cursor.ok()) {
                    cursor.fail(Result::INVALID_ARGUMENT);
                }
                return false;
            }
            saw_groups = true;
        }
        else {
            cursor.fail(Result::INVALID_ARGUMENT);
            return false;
        }
        if (!cursor.take(',')) {
            break;
        }
    }
    if (!cursor.take('}')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    if (!saw_service || !saw_instance || !saw_major || !saw_minor || !saw_unreliable ||
        !saw_reliable || !saw_groups) {
        cursor.fail(Result::INVALID_ARGUMENT);
        return false;
    }
    return true;
}

bool parse_service_array(Cursor& cursor, platform::Vector<DeployedService>& services)
{
    if (!cursor.take('[')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    if (cursor.take(']')) {
        return true;
    }
    for (;;) {
        DeployedService service;
        if (!parse_service_object(cursor, service)) {
            return false;
        }
        for (const auto& existing : services) {
            if (same_instance(existing, service.instance.service_id,
                              service.instance.instance_id)) {
                cursor.fail(Result::INVALID_ARGUMENT);
                return false;
            }
        }
        if (services.size() >= services.max_size()) {
            cursor.fail(Result::BUFFER_OVERFLOW);
            return false;
        }
        services.push_back(std::move(service));
        if (!cursor.take(',')) {
            break;
        }
    }
    if (!cursor.take(']')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    return true;
}

bool parse_milliseconds(Cursor& cursor, std::chrono::milliseconds& dest)
{
    uint64_t value = 0;
    if (!parse_bounded(cursor, 0xFFFFFFFFULL, value)) {
        return false;
    }
    dest = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(value));
    return true;
}

bool parse_sd_object(Cursor& cursor, SdConfig& config)
{
    if (!cursor.take('{')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    bool saw_group = false;
    bool saw_port = false;
    bool saw_min = false;
    bool saw_max = false;
    bool saw_base = false;
    bool saw_rep_max = false;
    bool saw_mult = false;
    bool saw_cyclic = false;
    bool saw_ttl = false;
    bool saw_unicast = false;
    bool saw_unicast_port = false;
    if (cursor.take('}')) {
        cursor.fail(Result::INVALID_ARGUMENT);
        return false;
    }
    for (;;) {
        std::array<char, MAX_TOKEN> key{};
        if (!parse_string(cursor, key) || !cursor.take(':')) {
            if (cursor.ok()) {
                cursor.fail(Result::MALFORMED_MESSAGE);
            }
            return false;
        }
        if (key_is(key.data(), "multicast_address")) {
            std::array<char, MAX_TOKEN> text{};
            if (saw_group || !parse_string(cursor, text)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            if (!store_string(config.multicast_address, text.data())) {
                cursor.fail(Result::BUFFER_OVERFLOW);
                return false;
            }
            saw_group = true;
        }
        else if (key_is(key.data(), "unicast_address")) {
            std::array<char, MAX_TOKEN> text{};
            if (saw_unicast || !parse_string(cursor, text)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            if (!store_string(config.unicast_address, text.data())) {
                cursor.fail(Result::BUFFER_OVERFLOW);
                return false;
            }
            saw_unicast = true;
        }
        else if (key_is(key.data(), "multicast_port")) {
            uint64_t value = 0;
            if (saw_port || !parse_bounded(cursor, 0xFFFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            config.multicast_port = static_cast<uint16_t>(value);
            saw_port = true;
        }
        else if (key_is(key.data(), "unicast_port")) {
            uint64_t value = 0;
            if (saw_unicast_port || !parse_bounded(cursor, 0xFFFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            config.unicast_port = static_cast<uint16_t>(value);
            saw_unicast_port = true;
        }
        else if (key_is(key.data(), "initial_delay_min_ms")) {
            if (saw_min || !parse_milliseconds(cursor, config.initial_delay_min)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            saw_min = true;
        }
        else if (key_is(key.data(), "initial_delay_max_ms")) {
            if (saw_max || !parse_milliseconds(cursor, config.initial_delay_max)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            saw_max = true;
        }
        else if (key_is(key.data(), "repetition_base_ms")) {
            if (saw_base || !parse_milliseconds(cursor, config.repetition_base)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            saw_base = true;
        }
        else if (key_is(key.data(), "repetition_max_ms")) {
            if (saw_rep_max || !parse_milliseconds(cursor, config.repetition_max)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            saw_rep_max = true;
        }
        else if (key_is(key.data(), "repetition_multiplier")) {
            uint64_t value = 0;
            if (saw_mult || !parse_bounded(cursor, 0xFFU, value)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            config.repetition_multiplier = static_cast<uint8_t>(value);
            saw_mult = true;
        }
        else if (key_is(key.data(), "cyclic_offer_ms")) {
            if (saw_cyclic || !parse_milliseconds(cursor, config.cyclic_offer)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            saw_cyclic = true;
        }
        else if (key_is(key.data(), "ttl_ms")) {
            if (saw_ttl || !parse_milliseconds(cursor, config.ttl)) {
                cursor.fail(cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error);
                return false;
            }
            saw_ttl = true;
        }
        else {
            cursor.fail(Result::INVALID_ARGUMENT);
            return false;
        }
        if (!cursor.take(',')) {
            break;
        }
    }
    if (!cursor.take('}')) {
        cursor.fail(Result::MALFORMED_MESSAGE);
        return false;
    }
    if (!saw_group || !saw_port || !saw_min || !saw_max || !saw_base || !saw_rep_max || !saw_mult ||
        !saw_cyclic || !saw_ttl) {
        cursor.fail(Result::INVALID_ARGUMENT);
        return false;
    }
    if (config.initial_delay_min > config.initial_delay_max) {
        cursor.fail(Result::INVALID_ARGUMENT);
        return false;
    }
    config.initial_delay = config.initial_delay_max;
    return true;
}

void apply_ports(DeployedService& service, bool provided, const platform::String<>& unicast)
{
    if (service.unreliable_port != 0) {
        service.instance.port = service.unreliable_port;
        service.instance.protocol = UDP_PROTOCOL;
        if (provided) {
            service.instance.ip_address = unicast;
        }
        return;
    }
    if (service.reliable_port != 0) {
        service.instance.port = service.reliable_port;
        service.instance.protocol = TCP_PROTOCOL;
    }
    if (provided) {
        service.instance.ip_address = unicast;
    }
}

Result parse_document(const char* text, std::size_t length, Deployment& out)
{
    Cursor cursor;
    cursor.cur = text;
    cursor.end = text + length;
    if (!cursor.take('{')) {
        return Result::MALFORMED_MESSAGE;
    }
    bool saw_sd = false;
    bool saw_provided = false;
    bool saw_required = false;
    if (cursor.take('}')) {
        return Result::INVALID_ARGUMENT;
    }
    for (;;) {
        std::array<char, MAX_TOKEN> key{};
        if (!parse_string(cursor, key) || !cursor.take(':')) {
            return cursor.ok() ? Result::MALFORMED_MESSAGE : cursor.error;
        }
        if (key_is(key.data(), "sd")) {
            if (saw_sd || !parse_sd_object(cursor, out.sd)) {
                return cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error;
            }
            saw_sd = true;
        }
        else if (key_is(key.data(), "provided_services")) {
            if (saw_provided || !parse_service_array(cursor, out.provided_services)) {
                return cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error;
            }
            saw_provided = true;
        }
        else if (key_is(key.data(), "required_services")) {
            if (saw_required || !parse_service_array(cursor, out.required_services)) {
                return cursor.ok() ? Result::INVALID_ARGUMENT : cursor.error;
            }
            saw_required = true;
        }
        else {
            return Result::INVALID_ARGUMENT;
        }
        if (!cursor.take(',')) {
            break;
        }
    }
    if (!cursor.take('}')) {
        return Result::MALFORMED_MESSAGE;
    }
    cursor.skip();
    if (cursor.cur != cursor.end) {
        return Result::MALFORMED_MESSAGE;
    }
    if (!saw_sd || !saw_provided || !saw_required) {
        return Result::INVALID_ARGUMENT;
    }
    for (auto& service : out.provided_services) {
        if (service.unreliable_port == 0 && service.reliable_port == 0) {
            return Result::INVALID_ARGUMENT;
        }
        apply_ports(service, true, out.sd.unicast_address);
    }
    for (auto& service : out.required_services) {
        apply_ports(service, false, out.sd.unicast_address);
    }
    return Result::SUCCESS;
}

}  // namespace

Result load_deployment(const char* text, std::size_t length, Deployment& out)
{
    out = Deployment{};
    if (text == nullptr && length != 0) {
        return Result::INVALID_ARGUMENT;
    }
    if (text == nullptr || length == 0) {
        return Result::MALFORMED_MESSAGE;
    }
    const Result result = parse_document(text, length, out);
    if (result != Result::SUCCESS) {
        out = Deployment{};
    }
    return result;
}

Result load_deployment_file(const char* path, Deployment& out)
{
    out = Deployment{};
    if (path == nullptr || path[0] == '\0') {
        return Result::INVALID_ARGUMENT;
    }
    std::FILE* file = std::fopen(path, "rb");  // NOLINT(cppcoreguidelines-owning-memory)
    if (file == nullptr) {
        return Result::INVALID_ARGUMENT;
    }
    std::array<char, DEPLOYMENT_FILE_MAX_BYTES> buffer{};
    const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), file);
    const int io_error = std::ferror(file);
    const int extra = (read == buffer.size()) ? std::fgetc(file) : EOF;
    const int closed = std::fclose(file);  // NOLINT(cppcoreguidelines-owning-memory)
    if (io_error != 0 || closed != 0) {
        return Result::INVALID_ARGUMENT;
    }
    if (extra != EOF) {
        return Result::BUFFER_OVERFLOW;
    }
    return load_deployment(buffer.data(), read, out);
}

}  // namespace someip::sd
