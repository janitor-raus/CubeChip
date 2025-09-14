/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "Millis.hpp"

#include <chrono>
#include <thread>

/*==================================================================*/

static const auto sInitialApplicationTimestamp
	{ std::chrono::steady_clock::now() };

/*==================================================================*/

long long Millis::now() noexcept {
	return std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::steady_clock::now() - sInitialApplicationTimestamp).count();
}

long long Millis::since(long long past_millis) noexcept {
	return now() - past_millis;
}

void Millis::sleep_for(unsigned long long millis) noexcept {
	std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

void Millis::sleeplock_for(double millis) noexcept {
	using namespace std::chrono;

	const auto target{ steady_clock::now() + duration_cast
		<nanoseconds>(duration<double, std::milli>(millis)) };

	while (true) {
		const auto cur_time{ steady_clock::now() };
		if (cur_time >= target) { return; }

		if (target - cur_time >= microseconds(2300)) // avg sleep duration 99.5% < 2.3ms
			{ std::this_thread::sleep_for(milliseconds(1)); }
		else
			{ std::this_thread::yield(); }
	}
}
