/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

/*==================================================================*/

namespace Millis {
	/**
	 * @brief Returns the current time in milliseconds since application start.
	 */
	[[nodiscard]]
	long long now() noexcept;
	[[nodiscard]]
	long long raw() noexcept;

	/**
	 * @brief Returns the difference between now() and past_millis in milliseconds.
	 */
	[[nodiscard]]
	long long since(long long past_millis) noexcept;

	/**
	 * @brief Naively sleeps the current thread for X milliseconds.
	 */
	void sleep_for(unsigned long long millis) noexcept;
	/**
	 * @brief Sleeps/spins the current thread for X milliseconds.
	 */
	void sleeplock_for(double millis) noexcept;
};
