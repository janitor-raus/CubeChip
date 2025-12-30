/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

/*==================================================================*/

namespace thread_affinity {
	/**
	 * @brief Attempt to grab the current process ID. Defaults to 0.
	 * @return Process ID number.
	 */
	unsigned get_process_id() noexcept;

	/**
	 * @brief Guesstimate of amount of logical cores the system has. Defaults to 1.
	 * @return Amount of logical cores.
	 */
	unsigned get_logical_core_count() noexcept;

	/**
	 * @brief Guesstimate of which logical processor core the current thread runs on. Defaults to 0.
	 * @return Processor core ID number.
	 */
	unsigned get_current_core() noexcept;

#if defined(_WIN32)
	/**
	 * @brief Sets thread affinity to desired logical cores. Will ignore invalid affinity masks safely.
	 * @param[in] affinity_mask :: Bitwise mask to control which logical cores are eligible.
	 * @param[in] thread_handle :: Thread handle pointer to (optionally) target a different thread, otherwise pick own thread.
	 * @return True if successful, false otherwise.
	 */
	bool set_affinity(unsigned long long affinity_mask, void* thread_handle = nullptr) noexcept;
#elif defined(__linux__)
	/**
	 * @brief Sets thread affinity to desired logical cores. Will ignore invalid affinity masks safely.
	 * @param[in] affinity_mask :: Bitwise mask to control which logical cores are eligible.
	 * @param[in] thread_handle :: Thread handle pointer to (optionally) target a different thread, otherwise pick own thread.
	 * @return True if successful, false otherwise.
	 */
	bool set_affinity(unsigned long long affinity_mask, void* thread_handle = nullptr) noexcept;
#elif defined(__APPLE__)
	/**
	 * @brief Sets thread affinity to desired tag. Tag "0" will clear an existing tag.
	 * @param[in] affinity_tag :: Tag value which controls the grouping of threads, if eligible.
	 * @param[in] thread_handle :: Thread handle pointer to (optionally) target a different thread, otherwise pick own thread.
	 * @return True if successful, false otherwise.
	 */
	bool set_affinity(unsigned long long affinity_tag, void* thread_handle = nullptr) noexcept;
#else
	/**
	 * @brief Unsupported in the current platform. Effectively a no-op.
	 */
	inline bool set_affinity(unsigned long long, void* = nullptr) noexcept { return false; }
#endif

/*==================================================================*/

	/**
	 * @brief
	 * Manages thread affinity by pinning a thread to a logical core group (hyperthread siblings)
	 * every time the system scheduler migrates the thread to a different core group, effective
	 * for a defined cooldown period. An affinity mask can be provided to avoid specific cores.
	 *
	 * Usage:
	 * - Construct a Manager object (ideally TLS) at the start of a thread or section of code.
	 * - Call `refresh_affinity()` periodically to enforce the pinning policy.
	 *
	 * Example:
	 * @code
	 * thread_affinity::Manager ag(100, 0b11); // 100 sec cooldown, avoid first two cores
	 * if (ag.refresh_affinity()) {
	 *     // thread was newly pinned, do something special
	 * }
	 * @endcode
	 */
	class Manager {
		long long timestamp{};
		unsigned  cooldown_p{};

		bool is_thread_pinned{ true };

		unsigned long long avoid_mask{};
		unsigned long long last_group{};

	public:
		Manager(
			unsigned           cooldown_p,
			unsigned long long avoid_mask = 0ull
		) noexcept;

		bool refresh_affinity() noexcept;
	};
}
