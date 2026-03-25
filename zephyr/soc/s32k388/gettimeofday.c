/*
 * Copyright (c) 2025 Vinicius Tadeu Zein
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal gettimeofday stub required by libstdc++'s chrono.o.
 * Uses Zephyr's k_uptime_get() as the time source.
 */

#include <sys/time.h>
#include <zephyr/kernel.h>

int gettimeofday(struct timeval *tv, void *tz)
{
	ARG_UNUSED(tz);

	if (tv != NULL) {
		int64_t ms = k_uptime_get();

		tv->tv_sec  = ms / 1000;
		tv->tv_usec = (ms % 1000) * 1000;
	}

	return 0;
}
