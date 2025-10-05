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
 *   SlidingRingBuffer<std::string, 256> buffer;
 *   buffer.push("hello");
 * @endcode
 */
template <typename T, std::size_t N = 8>
	requires (std::is_nothrow_default_constructible_v<T>)
class SlidingRingBuffer {
	static_assert((N & (N - 1)) == 0, "Buffer size (N) must be a power of two.");
	static_assert(N >= 16, "Buffer size (N) must be at least 16.");

	using self = SlidingRingBuffer;

	alignas(HDIS) AtomSharedPtr<T>  mBuffer[N]{};
	alignas(HDIS) Atom<std::size_t> mPushHead{ 0 };
	alignas(HDIS) Atom<std::size_t> mReadHead{ 0 };
	alignas(HDIS) mutable std::shared_mutex mGuard;

public:
	SlidingRingBuffer() noexcept { clear_buffer(); }

	SlidingRingBuffer(      self&&) = delete;
	SlidingRingBuffer(const self&)  = delete;

	self& operator=(      self&&) = delete;
	self& operator=(const self&)  = delete;

	constexpr auto size() const noexcept { return N; }
	constexpr auto head() const noexcept { return mReadHead.load(mo::acquire); }

private:
	void push_(std::size_t index, SharedPtr<T>&& ptr) noexcept {
		std::shared_lock lock{ mGuard };
		mBuffer[index & (N - 1)].store(std::move(ptr), mo::release);

		auto expected{ head() };
		while (expected < index && !mReadHead.compare_exchange_weak \
			(expected, index, mo::acq_rel, mo::acquire)) {}
	}

	T at_(std::size_t index) const noexcept
		{ return *mBuffer[index & (N - 1)].load(mo::acquire); }

	T at_(std::size_t index, std::size_t head) const noexcept
		{ return at_(head + N - index); }

private:
	class ProxySnap_Count {
		const self& buf;
		std::size_t count;

		friend class SlidingRingBuffer;
		ProxySnap_Count(const self& buf, std::size_t count) noexcept
			: buf{ buf }, count{ count } {}

		auto snapshot() const noexcept {
			const auto head{ buf.head() };
			if (head == 0) { return std::vector<T>{}; }

			const auto dist{ std::min(head + 1, N) };
			const auto size{ count ? std::min(count, dist) : dist };

			std::vector<T> output;
			output.reserve(size);

			for (std::size_t i{}; i < size; ++i)
				{ output.push_back(buf.at_(dist - size + i)); }

			return output;
		}

	public:
		ProxySnap_Count(      ProxySnap_Count&&) = delete;
		ProxySnap_Count(const ProxySnap_Count&)  = delete;
		ProxySnap_Count& operator=(      ProxySnap_Count&&) = delete;
		ProxySnap_Count& operator=(const ProxySnap_Count&)  = delete;

	public:
		[[nodiscard]] auto fast() const noexcept {
			return snapshot();
		}

		[[nodiscard]] auto safe() const noexcept {
			std::unique_lock lock{ buf.mGuard };
			return snapshot();
		}
	};

private:
	class ProxySnap_Range {
		const self& buf;
		std::size_t count, begin;

		friend class SlidingRingBuffer;
		ProxySnap_Range(const self& buf, std::size_t count, std::size_t begin) noexcept
			: buf{ buf }, count{ count }, begin{ begin & (N - 1) } {}

		auto snapshot() const noexcept {
			const auto head{ buf.head() };
			if (head == 0) { return std::vector<T>{}; }

			const auto relative{ head & (N - 1) };
			const auto offset{ std::min(begin, relative) };

			const auto dist{ (relative - offset) + 1 };
			const auto size{ count ? std::min(count, dist) : dist };

			std::vector<T> output;
			output.reserve(size);

			for (std::size_t i{}; i < size; ++i)
				{ output.push_back(buf.at_(offset + i)); }

			return output;
		}

	public:
		ProxySnap_Range(      ProxySnap_Range&&) = delete;
		ProxySnap_Range(const ProxySnap_Range&)  = delete;
		ProxySnap_Range& operator=(      ProxySnap_Range&&) = delete;
		ProxySnap_Range& operator=(const ProxySnap_Range&)  = delete;

		[[nodiscard]] auto fast() const noexcept {
			return snapshot();
		}

		[[nodiscard]] auto safe() const noexcept {
			std::unique_lock lock{ buf.mGuard };
			return snapshot();
		}
	};

public:
	/**
	 * @brief Empty the internal buffer by default-initializing its contents.
	 * @note This method is thread-safe, but cannot run concurrently with push() or safe_snapshot_*() calls.
	 */
	void clear_buffer() noexcept {
		std::unique_lock lock{ mGuard };
		const static auto sDefaultT
			{ std::make_shared<T>(T{}) };

		for (auto& entry : mBuffer)
			{ entry.store(sDefaultT, mo::relaxed); }
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
	 * @brief Retrieve a snapshot of `count` entries from the internal buffer.
	 *        Entries are relative to the most recent push() call.
	 *        Follow up with .fast() or .safe() to get the data.
	 * @param[in] count :: Amount of entries to retrieve, or 0 for all available.
	 * @return A proxy object exposing .fast() and .safe() methods to retrieve the data.
	 */
	auto snapshot(std::size_t count) const noexcept
		{ return ProxySnap_Count(*this, count); }

	/**
	 * @brief Retrieve a snapshot of `count` entries from the internal buffer.
	 *        Entries are relative to the provided `begin` index value.
	 *        Follow up with .fast() or .safe() to get the data.
	 * @param[in] count :: Amount of entries to retrieve, or 0 for all available.
	 * @param[in] begin :: Custom index into the buffer, clamped to the most recent push() call.
	 * @return A proxy object exposing .fast() and .safe() methods to retrieve the data.
	 */
	auto snapshot(std::size_t count, std::size_t begin) const noexcept
		{ return ProxySnap_Range(*this, count, begin); }
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
