/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "ThreadAffinity.hpp"
#include "Millis.hpp"

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#elif defined(__linux__)
	#include <thread>
	#include <algorithm>
	#include <pthread.h>
	#include <sched.h>
	#include <unistd.h>
#elif defined(__APPLE__)
	#include <thread>
	#include <algorithm>
	#include <pthread.h>
	#include <unistd.h>
	#include <sys/sysctl.h>
	#include <mach/mach.h>
	#include <mach/thread_policy.h>
#endif

/*==================================================================*/

unsigned thread_affinity::get_process_id() noexcept {
#if defined(_WIN32)
	return static_cast<unsigned>(GetCurrentProcessId());
#elif defined(__linux__) || defined(__APPLE__)
	return static_cast<unsigned>(getpid());
#else
	return 0; // Web or unknown
#endif
}

unsigned thread_affinity::get_logical_core_count() noexcept {
#if defined(_WIN32)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#elif defined(__linux__) || defined(__APPLE__)
	return std::max(1u, std::thread::hardware_concurrency()); // fallback
#else
	return 1; // Web or unknown
#endif
}

unsigned thread_affinity::get_current_core() noexcept {
#if defined(_WIN32)
	return GetCurrentProcessorNumber();
#elif defined(__linux__)
	return std::max(0, sched_getcpu());
#else
	return 0; // MacOS or unknown
#endif
}

#if defined(_WIN32)
	bool thread_affinity::set_affinity(unsigned long long affinity_mask, void* thread_handle) noexcept {
		const auto cpu_count = get_logical_core_count();

		const auto valid_mask = cpu_count >= 64 ? affinity_mask
			: affinity_mask & ((1ull << cpu_count) - 1);

		if (valid_mask == 0x0) { return false; }

		auto thread = thread_handle ? static_cast<HANDLE>(thread_handle) : GetCurrentThread();
		return SetThreadAffinityMask(thread, static_cast<DWORD_PTR>(valid_mask)) != 0;
	}
#elif defined(__linux__)
	bool thread_affinity::set_affinity(unsigned long long affinity_mask, void* thread_handle) noexcept {
		const auto cpu_count = std::min(get_logical_core_count(),
			static_cast<unsigned>(CPU_SETSIZE));

		const auto valid_mask = cpu_count >= 64 ? affinity_mask
			: affinity_mask & ((1ull << cpu_count) - 1);

		if (valid_mask == 0) { return false; }

		cpu_set_t set; CPU_ZERO(&set);

		for (auto i = 0; i < cpu_count; ++i) {
			if (valid_mask & (1ull << i)) { CPU_SET(i, &set); }
		}

		auto thread = thread_handle ? *static_cast<pthread_t*>(thread_handle) : pthread_self();
		return pthread_setaffinity_np(thread, sizeof(set), &set) == 0;
	}
#elif defined(__APPLE__)
	bool thread_affinity::set_affinity(unsigned long long affinity_tag, void* thread_handle) noexcept {
		auto tag = static_cast<integer_t>(affinity_tag);
		auto thread = thread_handle ? *static_cast<pthread_t*>(thread_handle) : pthread_self();

		return thread_policy_set(
			pthread_mach_thread_np(thread), THREAD_AFFINITY_POLICY,
			&tag, THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;
	}
#else
	bool thread_affinity::set_affinity(unsigned long long, void*) noexcept { return false; }
#endif

/*==================================================================*/

static auto get_core_group() noexcept {
	const auto core_affinity_group = thread_affinity::get_current_core() & ~1u;
	return 1ull << core_affinity_group | 1ull << (core_affinity_group + 1);
}

thread_affinity::Manager::Manager(
	unsigned           cooldown_p,
	unsigned long long avoid_mask
) noexcept
	: timestamp(-Millis::now())
	, cooldown_p(cooldown_p * 1000)
	, avoid_mask(avoid_mask)
{}

#if defined(_WIN32) || defined(__linux__)
	bool thread_affinity::Manager::refresh_affinity() noexcept {
		if (is_thread_pinned) {
			if (Millis::since(timestamp) >= cooldown_p) {
				set_affinity(~avoid_mask);
				is_thread_pinned = false;
			}
		} else {
			auto this_group = ::get_core_group();
			if (this_group != last_group) {
				last_group = this_group;
				timestamp  = Millis::now();
				set_affinity(this_group);
				is_thread_pinned = true;
				return true;
			}
		}
		return false;
	}
#else
	bool thread_affinity::Manager::refresh_affinity() noexcept { return false; }
#endif
