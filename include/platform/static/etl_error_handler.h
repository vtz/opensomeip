/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOMEIP_PLATFORM_STATIC_ETL_ERROR_HANDLER_H
#define SOMEIP_PLATFORM_STATIC_ETL_ERROR_HANDLER_H

/**
 * @brief Custom ETL error handler for safety-critical static allocation.
 *
 * When ETL_LOG_ERRORS is enabled and ETL_THROW_EXCEPTIONS is 0, ETL calls
 * etl::error_handler::error() on assertion failures (e.g. container overflow).
 * This module registers a callback that logs and returns (no abort) so the
 * system degrades gracefully per FMEA §3.2.
 *
 * @implements REQ_PAL_ETL_ERROR_HANDLER
 */

#include <cstdint>

namespace someip::platform {

void register_etl_error_handler();

uint32_t get_etl_error_count();

void reset_etl_error_count();

}  // namespace someip::platform

#endif // SOMEIP_PLATFORM_STATIC_ETL_ERROR_HANDLER_H
