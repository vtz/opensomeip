/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * @implements REQ_PLATFORM_STATIC_ETL_HANDLER
 */

#include "etl_error_handler.h"

#include <atomic>
#include <cstdint>

#if defined(ETL_LOG_ERRORS) || defined(ETL_IN_UNIT_TEST)
#include <etl/error_handler.h>
#endif

namespace someip::platform {

namespace {

std::atomic<uint32_t> error_count{0};

#if defined(ETL_LOG_ERRORS) || defined(ETL_IN_UNIT_TEST)
void on_etl_error(const etl::exception& /*e*/) {
    error_count.fetch_add(1, std::memory_order_relaxed);
}
#endif

}  // namespace

void register_etl_error_handler() {
#if defined(ETL_LOG_ERRORS) || defined(ETL_IN_UNIT_TEST)
    etl::error_handler::set_callback<on_etl_error>();
#endif
}

uint32_t get_etl_error_count() {
    return error_count.load(std::memory_order_relaxed);
}

void reset_etl_error_count() {
    error_count.store(0, std::memory_order_relaxed);
}

}  // namespace someip::platform
