/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <mutex>
#include <shared_mutex>
#include <cstdint>
#include <algorithm>

#include "Concepts.hpp"
#include "Aligned.hpp"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4324)
#endif

/*==================================================================*/

/**
 * @brief A thread-safe, Mailbox-style Triple Buffer implementation
 *        that operates flexibly with a given fixed-size Buffer type.
 *
 * TripleBufferX maintains three copies of the Buffer:
 *   - A read buffer (for consumers)
 *   - A work buffer (for producers)
 *   - A swap buffer (for atomic publishing)
 *
 * The class provides single-shot access methods:
 *   - read(Fn&&) - locks the read buffer, invokes a callable with an
 *     auto argument exposing const access to the buffer and
 *     a compile-time dirty flag.
 *   - write(Fn&&) - locks the work buffer, invokes a callable with a
 *     mutable auto& argument, and publishes the buffer atomically
 *     after the callable returns.
 *
 * Thread-safety:
 *   - Reads and writes are independently locked.
 *   - Dirty-flag propagation ensures consumers know when new data is available.
 */
template <class Buffer>
class TripleBufferX {
	struct alignas(HDIS) StateBody {
		using AtomBuf = std::atomic<Buffer*>;

		Buffer mWorkBuffer;
		Buffer mReadBuffer;
		Buffer mSwapBuffer;

		alignas(HDIS) mutable std::mutex mReadLock;
		alignas(HDIS) mutable std::mutex mWorkLock;

		alignas(HDIS) mutable Buffer* mpWork = &mWorkBuffer;
		alignas(HDIS) mutable AtomBuf mpRead = &mReadBuffer;
		alignas(HDIS) mutable AtomBuf mpSwap = &mSwapBuffer;

		template <typename... Args>
		StateBody(Args&&... args) noexcept(std::is_nothrow_constructible_v<Buffer, Args...>)
			: mWorkBuffer(std::forward<Args>(args)...)
			, mReadBuffer(std::forward<Args>(args)...)
			, mSwapBuffer(std::forward<Args>(args)...) {
		}
	};

	std::unique_ptr<StateBody>
		mState;

private:
	static constexpr std::uintptr_t sNewDataFlag = 1ull;

	static_assert((alignof(Buffer) & sNewDataFlag) == 0,
		"TripleBufferX: Buffer alignment must permit pointer tagging (LSB == 0).");

	constexpr bool getFlag(Buffer* ptr) const noexcept {
		return reinterpret_cast<std::uintptr_t>(ptr) & sNewDataFlag;
	}

	constexpr Buffer* addFlag(Buffer* ptr) const noexcept {
		auto new_ptr = reinterpret_cast<std::uintptr_t>(ptr);
		return reinterpret_cast<Buffer*>(new_ptr | sNewDataFlag);
	}

	constexpr Buffer* subFlag(Buffer* ptr) const noexcept {
		auto new_ptr = reinterpret_cast<std::uintptr_t>(ptr);
		return reinterpret_cast<Buffer*>(new_ptr & ~sNewDataFlag);
	}


public:
	template <typename... Args>
	TripleBufferX(Args&&... args) noexcept(std::is_nothrow_constructible_v<Buffer, Args...>)
		: mState(std::make_unique<StateBody>(std::forward<Args>(args)...))
	{}

	explicit TripleBufferX(TripleBufferX&&) noexcept = default;
	TripleBufferX& operator=(TripleBufferX&& other) noexcept = default;

	explicit TripleBufferX(const TripleBufferX&) = delete;
	TripleBufferX& operator=(const TripleBufferX&) = delete;

private:
	template <bool DIRTY>
	struct BufferView {
		static constexpr bool dirty = DIRTY;
		const Buffer& buffer;
	};


public:
	/**
	 * @brief Provides read-only access to the read buffer in a thread-safe,
	 *        single-shot manner.
	 *
	 * The callable is invoked with a single 'BufferView<DIRTY>' argument,
	 * and exposes the following access methods:
	 *
	 *   - dirty - static bool, marks whether buffer was updated (dirty) since last read.
	 *   - buffer - const reference to the underlying buffer object.
	 *
	 * Callers are recommended to accept the argument as auto to handle both
	 * 'BufferView<true>' and 'BufferView<false>' cases transparently.
	 *
	 * @tparam Fn  A callable that can be invoked with 'auto' and may return
	 *             any type (including 'void').
	 *
	 * @param function  The callable to execute under the read lock.
	 *
	 * @return decltype(auto) - forwards whatever the callable returns.
	 *
	 * Thread-safety: The read buffer is locked for the entire duration
	 *                of the callable.
	 */
	template <typename Fn> requires (
		std::is_invocable_v<Fn, BufferView<true>> &&
		std::is_invocable_v<Fn, BufferView<false>>
	)
	decltype(auto) present(Fn&& function) const noexcept(
		std::is_nothrow_invocable_v<Fn, BufferView<true>> &&
		std::is_nothrow_invocable_v<Fn, BufferView<false>>
	) {
		std::unique_lock lock(mState->mReadLock);

		if (getFlag(mState->mpSwap.load(std::memory_order::acquire))) {
			mState->mpRead.store(subFlag(mState->mpSwap.exchange(mState->mpRead,
				std::memory_order::acq_rel)), std::memory_order::release);

			return std::forward<Fn>(function)(
				BufferView<true>{ *mState->mpRead.load(std::memory_order::acquire) });
		} else {
			return std::forward<Fn>(function)(
				BufferView<false>{ *mState->mpRead.load(std::memory_order::acquire) });
		}
	}

	/**
	 * @brief Provides write access to the work buffer in a thread-safe,
	 *        single-shot manner.
	 *
	 * The callable is invoked with a single 'Buffer&' argument pointing to the
	 * writable work buffer. The callable may freely modify the contents of this
	 * buffer. Once it completes, the buffer is safely published as the new swap
	 * target.
	 *
	 * Callers are recommended to accept the argument explicitly as 'auto&'.
	 *
	 * @tparam Fn  A callable that can be invoked with 'auto&' and may return
	 *             any type (including 'void').
	 *
	 * @param function  The callable to execute under the work lock.
	 *
	 * @return decltype(auto) - forwards whatever the callable returns.
	 *
	 * Thread-safety: The work buffer is locked for the entire duration of the
	 *                callable and is published atomically afterward.
	 */
	template <typename Fn> requires (std::is_invocable_v<Fn, Buffer&>)
	decltype(auto) acquire(Fn&& function) noexcept(
		std::is_nothrow_invocable_v<Fn, Buffer&>
	) {
		std::unique_lock lock(mState->mWorkLock);

		if constexpr (std::is_void_v<std::invoke_result_t<Fn, Buffer&>>) {
			std::forward<Fn>(function)(*mState->mpWork);
			mState->mpWork = subFlag(mState->mpSwap.exchange(addFlag(mState->mpWork), std::memory_order::acq_rel));
		}
		else {
			decltype(auto) result = std::forward<Fn>(function)(*mState->mpWork);
			mState->mpWork = subFlag(mState->mpSwap.exchange(addFlag(mState->mpWork), std::memory_order::acq_rel));
			return result;
		}
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
