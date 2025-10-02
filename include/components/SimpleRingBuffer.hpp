/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <vector>
#include <algorithm>
#include <utility>
#include <mutex>
#include <shared_mutex>
#include <type_traits>

#include "HDIS_HCIS.hpp"
#include "AtomSharedPtr.hpp"
#include "ExecPolicy.hpp"

/*==================================================================*/

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4324)
#endif

/**
 * @brief A lock-free, multi-producer, multi-consumer ring buffer.
 * @tparam T :: The type stored in the buffer. Must be nothrow default constructible.
 * @tparam N :: The number of slots in the buffer. Must be a power of two, and at least 8.
 * @details This ring buffer allows concurrent push and read operations from multiple threads.
 *          Internally uses atomic operations and shared pointers to manage lifetime.
 * @note Values passed to the push() method must be same as or convertible to `T`.
 * @code{.cpp}
 *   SimpleRingBuffer<std::string, 256> buffer;
 *   buffer.push("hello");
 * @endcode
 */
template <typename T, std::size_t N = 8>
	requires (std::is_nothrow_default_constructible_v<T>)
class SimpleRingBuffer {
	static_assert((N & (N - 1)) == 0, "Buffer size (N) must be a power of two.");
	static_assert(N >= 8, "Buffer size (N) must be at least 8.");

	using self = SimpleRingBuffer;

	alignas(HDIS) AtomSharedPtr<T>  mBuffer[N]{};
	alignas(HDIS) Atom<std::size_t> mPushHead{ 0 };
	alignas(HDIS) Atom<std::size_t> mReadHead{ 0 };
	alignas(HDIS) mutable std::shared_mutex mGuard;

public:
	SimpleRingBuffer() noexcept { clear_buffer(); }

	SimpleRingBuffer(self&&) = delete;
	SimpleRingBuffer(const self&) = delete;

	self& operator=(self&&) = delete;
	self& operator=(const self&) = delete;

	constexpr auto size() const noexcept { return N; }
	constexpr auto head() const noexcept { return mReadHead.load(mo::acquire); }

private:
	enum class SnapshotOrder { DESCENDING, ASCENDING };

	void push_(std::size_t index, SharedPtr<T>&& ptr) noexcept {
		std::shared_lock lock{ mGuard };
		mBuffer[index & (N - 1)].store(std::move(ptr), mo::release);

		auto expected{ head() };
		while (expected < index && !mReadHead.compare_exchange_weak \
			(expected, index, mo::acq_rel, mo::acquire)) {}
	}

	auto at_(std::size_t index, std::size_t head) const noexcept {
		return *mBuffer[(head + N - index) & (N - 1)].load(mo::acquire);
	}

	template <SnapshotOrder Order>
	auto snapshot_(std::size_t count) const noexcept {
		const auto pos{ head() };
		const auto max{ std::min(pos + 1, N) };
		std::vector<T> output(count ? std::min(count, max) : max);

		if constexpr (Order == SnapshotOrder::ASCENDING) {
			std::for_each_n(EXEC_POLICY(unseq)
				output.begin(), output.size(),
				[&](auto& entry) noexcept {
					entry = at_(max - 1 - (&entry - output.data()), pos); }
			);
		} else {
			std::for_each_n(EXEC_POLICY(unseq)
				output.begin(), output.size(),
				[&](auto& entry) noexcept {
					entry = at_(&entry - output.data(), pos); }
			);
		}

		return output;
	}

public:
	/**
	 * @brief Empty the internal buffer by default-initializing its contents.
	 * @note This method is thread-safe, but cannot run concurrently with push() or safe_snapshot_*() calls.
	 */
	void clear_buffer() noexcept {
		std::unique_lock lock{ mGuard };
		const static auto sDefaultT
			{ std::make_shared<T>(T{}) };

		std::for_each_n(EXEC_POLICY(unseq)
			mBuffer, N, [](auto& entry) noexcept
				{ entry.store(sDefaultT, mo::relaxed); }
		);
	}

	/**
	 * @brief Push a new value into the internal buffer. Each call advances the relative index
	 *        used by at() calls in a monotonic fashion.
	 * @param[in] value :: Value to copy or move into the internal buffer.
	 * @note This method is thread-safe, but cannot run concurrently with clear() or safe_snapshot_*() calls.
	 */
	void push(auto&& value) noexcept {
		using U = decltype(value);
		static_assert(std::is_convertible_v<U, T>,
			"Pushed value is not convertible to buffer type.");

		push_(mPushHead.fetch_add(1, mo::acq_rel),
			std::make_shared<T>(std::forward<U>(value)));
	}

	/**
	 * @brief Retrieve a copy of a value from the internal buffer. The index is relative to the
	 *        the most recent push() call, with 0 being the most recent entry.
	 * @param[in] index :: Offset relative to the most recent entry.
	 * @note This method is thread-safe, but may return stale data due to its non-blocking nature.
	 */
	T at(std::size_t index) const noexcept {
		return at_(index, head());
	}

	/**
	 * @brief Create a non-blocking snapshot of the internal buffer's contents. Values are ordered
	 *        from oldest first (ascending) or newest first (descending) at the time of the call.
	 * @returns A vector of T representing the buffer contents; missing values are default-constructed.
	 * @note This method is thread-safe, but may return stale data due to its non-blocking nature.
	 */
	std::vector<T> fast_snapshot_asc(std::size_t count = 0) const noexcept {
		return snapshot_<SnapshotOrder::ASCENDING>(count);
	}
	std::vector<T> fast_snapshot_desc(std::size_t count = 0) const noexcept {
		return snapshot_<SnapshotOrder::DESCENDING>(count);
	}

	/**
	 * @brief Create a blocking snapshot of the internal buffer's contents. Values are ordered
	 *        from oldest first (ascending) or newest first (descending) at the time of the call.
	 * @returns A vector of T representing the buffer contents; missing values are default-constructed.
	 * @note This method is thread-safe, but cannot run concurrently with push() or clear() calls.
	 */
	std::vector<T> safe_snapshot_asc(std::size_t count = 0) const noexcept {
		std::unique_lock lock{ mGuard };
		return snapshot_<SnapshotOrder::ASCENDING>(count);
	}
	std::vector<T> safe_snapshot_desc(std::size_t count = 0) const noexcept {
		std::unique_lock lock{ mGuard };
		return snapshot_<SnapshotOrder::DESCENDING>(count);
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
