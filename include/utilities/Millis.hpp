/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

/*==================================================================*/

namespace Millis {
	/**
	 * @brief Returns the initial timestamp in nanoseconds since epoch.
	 */
	[[nodiscard]]
	long long initial() noexcept;

	/**
	 * @brief Returns the initial wall timestamp in nanoseconds since epoch.
	 */
	[[nodiscard]]
	long long initial_wall() noexcept;

	/**
	 * @brief Returns the current time in milliseconds since application start.
	 */
	[[nodiscard]]
	long long now() noexcept;
	[[nodiscard]]
	long long raw() noexcept;

	/**
	 * @brief Returns the current wall time in milliseconds since application start.
	 */
	[[nodiscard]]
	long long now_wall() noexcept;
	[[nodiscard]]
	long long raw_wall() noexcept;

	/**
	 * @brief Returns the difference between now() and past_millis in milliseconds.
	 */
	[[nodiscard]]
	long long since(long long past_millis) noexcept;

	/**
	 * @brief Returns the difference between now_wall() and past_millis in milliseconds.
	 */
	[[nodiscard]]
	long long since_wall(long long past_millis) noexcept;

	/**
	 * @brief Naively sleeps the current thread for X milliseconds.
	 */
	void sleep_for(unsigned long long millis) noexcept;
	/**
	 * @brief Sleeps/spins the current thread for X milliseconds.
	 */
	void sleeplock_for(double millis) noexcept;
};

/*==================================================================*/

#include <string>

struct NanoTime {
	const long long nano{};

	constexpr NanoTime(long long n) noexcept : nano(n) {}

	constexpr operator long long() const noexcept { return nano; }

	std::string format_as_timer() const noexcept;
	std::string format_as_datetime(const char* fmt_string) const noexcept;
};
