/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "Millis.hpp"
#include "RelaxCPU.hpp"

#include <chrono>
#include <thread>

/*==================================================================*/

static const auto s_initial_app_timestamp
	= std::chrono::steady_clock::now();

static const auto s_initial_app_timestamp_wall
	= std::chrono::system_clock::now();

/*==================================================================*/

long long Millis::initial() noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds> \
		(s_initial_app_timestamp.time_since_epoch()).count();
}

long long Millis::initial_wall() noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds> \
		(s_initial_app_timestamp_wall.time_since_epoch()).count();
}

long long Millis::now() noexcept {
	return std::chrono::duration_cast<std::chrono::milliseconds> \
		(std::chrono::steady_clock::now() - s_initial_app_timestamp).count();
}

long long Millis::raw() noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds> \
		(std::chrono::steady_clock::now() - s_initial_app_timestamp).count();
}

long long Millis::now_wall() noexcept {
	return std::chrono::duration_cast<std::chrono::milliseconds> \
		(std::chrono::system_clock::now() - s_initial_app_timestamp_wall).count();
}

long long Millis::raw_wall() noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds> \
		(std::chrono::system_clock::now() - s_initial_app_timestamp_wall).count();
}

long long Millis::since(long long past_millis) noexcept {
	return now() - past_millis;
}

long long Millis::since_wall(long long past_millis) noexcept {
	return now_wall() - past_millis;
}

void Millis::sleep_for(unsigned long long millis) noexcept {
	std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

void Millis::sleeplock_for(double millis) noexcept {
	using namespace std::chrono;

	const auto target = steady_clock::now() + duration_cast \
		<nanoseconds>(duration<double, std::milli>(millis));

	while (true) {
		const auto cur_time = steady_clock::now();
		if (cur_time >= target) { return; }

		// avg 1ms sleep duration is 99.5% below 2.3ms
		if (target - cur_time >= microseconds(2300)) {
			std::this_thread::sleep_for(milliseconds(1));
		} else {
			for (auto i = 0; ++i <= 128;) { ::cpu_relax(); }
			std::this_thread::yield();
		}
	}
}

/*==================================================================*/

#include <fmt/format.h>
#include <fmt/chrono.h>

std::string NanoTime::format_as_timer() const noexcept {
	const auto total_secs = nano / 1'000'000'000ll;

	return fmt::format("{}:{:02}:{:02}.{:03}",
		total_secs / 3600ll, (total_secs / 60ll) % 60ll,
		total_secs % 60ll, (nano / 1000000ll) % 1000ll);
}

std::string NanoTime::format_as_datetime(const char* fmt_string) const noexcept {
	using namespace std::chrono;

	std::tm local_tm;
	auto timestamp = system_clock::to_time_t(system_clock::time_point(
		duration_cast<system_clock::duration>(seconds(nano / 1'000'000'000))));

#if defined(_WIN32) && !defined(__CYGWIN__)
	::localtime_s(&local_tm, &timestamp);
#else
	::localtime_r(&timestamp, &local_tm);
#endif

	return fmt::vformat(fmt_string, fmt::make_format_args(local_tm));
}
