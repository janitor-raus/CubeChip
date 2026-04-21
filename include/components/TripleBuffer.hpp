/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <mutex>
#include <cstdint>
#include <type_traits>

#include "HDIS_HCIS.hpp"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4324)
#endif

/*==================================================================*/

/**
 * @brief A thread-safe, Mailbox-style Triple Buffer implementation
 *        that operates flexibly with a given fixed-size Buffer type.
 *
 * TripleBuffer maintains three copies of the Buffer:
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
class TripleBuffer {
	struct alignas(HDIS) TripleBufferContext {
		using AtomBuf = std::atomic<Buffer*>;

		alignas(HDIS) Buffer m_work_buffer;
		alignas(HDIS) Buffer m_read_buffer;
		alignas(HDIS) Buffer m_swap_buffer;

		alignas(HDIS) mutable std::mutex m_reader_lock;
		mutable std::atomic<std::size_t> m_present_count{};
		alignas(HDIS) mutable std::mutex m_worker_lock;
		mutable std::atomic<std::size_t> m_acquire_count{};

		alignas(HDIS) mutable Buffer* m_work_ptr = &m_work_buffer;
		alignas(HDIS) mutable AtomBuf m_read_ptr = &m_read_buffer;
		alignas(HDIS) mutable AtomBuf m_swap_ptr = &m_swap_buffer;

		template <typename... Args>
		TripleBufferContext(Args&&... args) noexcept(std::is_nothrow_constructible_v<Buffer, Args...>)
			: m_work_buffer(std::forward<Args>(args)...)
			, m_read_buffer(std::forward<Args>(args)...)
			, m_swap_buffer(std::forward<Args>(args)...)
		{}
	};

	std::unique_ptr<TripleBufferContext>
		m_context;


private:
	static constexpr std::uintptr_t s_dirty_flag = 1ull;

	static_assert((alignof(Buffer) & s_dirty_flag) == 0,
		"TripleBuffer: Buffer alignment must permit pointer tagging (LSB == 0).");

	static constexpr bool get_dirty(Buffer* ptr) noexcept {
		return reinterpret_cast<std::uintptr_t>(ptr) & s_dirty_flag;
	}

	static constexpr Buffer* add_dirty(Buffer* ptr) noexcept {
		auto new_ptr = reinterpret_cast<std::uintptr_t>(ptr);
		return reinterpret_cast<Buffer*>(new_ptr | s_dirty_flag);
	}

	static constexpr Buffer* sub_dirty(Buffer* ptr) noexcept {
		auto new_ptr = reinterpret_cast<std::uintptr_t>(ptr);
		return reinterpret_cast<Buffer*>(new_ptr & ~s_dirty_flag);
	}


public:
	template <typename... Args>
	TripleBuffer(Args&&... args) noexcept(std::is_nothrow_constructible_v<Buffer, Args...>)
		: m_context(std::make_unique<TripleBufferContext>(std::forward<Args>(args)...))
	{}

	auto get_present_count() const noexcept { return m_context->m_present_count.load(std::memory_order::acquire); }
	auto get_acquire_count() const noexcept { return m_context->m_acquire_count.load(std::memory_order::acquire); }

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
		std::scoped_lock lock(m_context->m_reader_lock);
		m_context->m_present_count.fetch_add(1, std::memory_order::acq_rel);

		if (get_dirty(m_context->m_swap_ptr.load(std::memory_order::acquire))) {
			m_context->m_read_ptr.store(sub_dirty(m_context->m_swap_ptr.exchange(
				m_context->m_read_ptr, std::memory_order::acq_rel)), std::memory_order::release);

			return std::forward<Fn>(function)(
				BufferView<true>{ *m_context->m_read_ptr.load(std::memory_order::acquire) });
		} else {
			return std::forward<Fn>(function)(
				BufferView<false>{ *m_context->m_read_ptr.load(std::memory_order::acquire) });
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
		std::scoped_lock lock(m_context->m_worker_lock);
		m_context->m_acquire_count.fetch_add(1, std::memory_order::acq_rel);

		if constexpr (std::is_void_v<std::invoke_result_t<Fn, Buffer&>>) {
			std::forward<Fn>(function)(*m_context->m_work_ptr);
			m_context->m_work_ptr = sub_dirty(m_context->m_swap_ptr.exchange(
				add_dirty(m_context->m_work_ptr), std::memory_order::acq_rel));
		}
		else {
			decltype(auto) result = std::forward<Fn>(function)(*m_context->m_work_ptr);
			m_context->m_work_ptr = sub_dirty(m_context->m_swap_ptr.exchange(
				add_dirty(m_context->m_work_ptr), std::memory_order::acq_rel));
			return result;
		}
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
