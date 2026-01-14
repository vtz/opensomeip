#include "common/result.h"
#include <unordered_map>

namespace someip {

std::string to_string(Result result) {
    static const std::unordered_map<Result, std::string> result_strings = {
        {Result::SUCCESS, "SUCCESS"},
        {Result::NETWORK_ERROR, "NETWORK_ERROR"},
        {Result::CONNECTION_LOST, "CONNECTION_LOST"},
        {Result::CONNECTION_REFUSED, "CONNECTION_REFUSED"},
        {Result::TIMEOUT, "TIMEOUT"},
        {Result::INVALID_ENDPOINT, "INVALID_ENDPOINT"},
        {Result::INVALID_MESSAGE, "INVALID_MESSAGE"},
        {Result::INVALID_MESSAGE_TYPE, "INVALID_MESSAGE_TYPE"},
        {Result::INVALID_SERVICE_ID, "INVALID_SERVICE_ID"},
        {Result::INVALID_METHOD_ID, "INVALID_METHOD_ID"},
        {Result::INVALID_PROTOCOL_VERSION, "INVALID_PROTOCOL_VERSION"},
        {Result::INVALID_INTERFACE_VERSION, "INVALID_INTERFACE_VERSION"},
        {Result::MALFORMED_MESSAGE, "MALFORMED_MESSAGE"},
        {Result::INVALID_SESSION_ID, "INVALID_SESSION_ID"},
        {Result::SESSION_EXPIRED, "SESSION_EXPIRED"},
        {Result::SESSION_NOT_FOUND, "SESSION_NOT_FOUND"},
        {Result::OUT_OF_MEMORY, "OUT_OF_MEMORY"},
        {Result::BUFFER_OVERFLOW, "BUFFER_OVERFLOW"},
        {Result::RESOURCE_EXHAUSTED, "RESOURCE_EXHAUSTED"},
        {Result::SERVICE_NOT_FOUND, "SERVICE_NOT_FOUND"},
        {Result::SERVICE_UNAVAILABLE, "SERVICE_UNAVAILABLE"},
        {Result::SUBSCRIPTION_FAILED, "SUBSCRIPTION_FAILED"},
        {Result::SAFETY_VIOLATION, "SAFETY_VIOLATION"},
        {Result::FAULT_DETECTED, "FAULT_DETECTED"},
        {Result::RECOVERY_FAILED, "RECOVERY_FAILED"},
        {Result::NOT_IMPLEMENTED, "NOT_IMPLEMENTED"},
        {Result::INVALID_ARGUMENT, "INVALID_ARGUMENT"},
        {Result::PERMISSION_DENIED, "PERMISSION_DENIED"},
        {Result::INTERNAL_ERROR, "INTERNAL_ERROR"},
        {Result::NOT_INITIALIZED, "NOT_INITIALIZED"},
        {Result::INVALID_STATE, "INVALID_STATE"},
        {Result::UNKNOWN_ERROR, "UNKNOWN_ERROR"}
    };

    auto it = result_strings.find(result);
    return (it != result_strings.end()) ? it->second : "UNKNOWN_RESULT";
}

} // namespace someip
