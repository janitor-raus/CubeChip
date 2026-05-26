/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <atomic>
#include <memory>
#include <utility>
#include <type_traits>

#include "HDIS_HCIS.hpp"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4324)
#endif

#ifdef TRIPLE_BUFFER_ENFORCE_SPSC
	#include <mutex>
#endif

/*==================================================================*/

/**
 * @brief A thread-safe, flexible, Mailbox-style triple-buffer implementation.
 *
 * TripleBuffer maintains three instances of a given Buffer type:
 *   - A reader Buffer (for consumers)
 *   - A writer Buffer (for producers)
 *   - A middle Buffer (for atomic swap & publish)
 *
 * The class provides single-shot access methods:
 *   - present(Fn&&) - accesses the reader Buffer and invokes a callable with an
 *     'auto' argument exposing const access to the Buffer and a compile-time dirty flag.
 *   - acquire(Fn&&) - accesses the writer Buffer, invokes a callable with a
 *     mutable 'auto&' argument, and publishes the Buffer atomically after completion.
 *
 * Thread-safety:
 *   - Concurrent producer/consumer usage is safe under the intended SPSC model.
 *   - Dirty-flag propagation allows the consumer to determine whether new data
 *     has been published since the last 'present()' call.
 *
 * @note This implementation assumes the developer will respect SPSC usage patterns.
 *       If stricter enforcement is required, define the compile-time
 *       'TRIPLE_BUFFER_ENFORCE_SPSC' macro before including this header. This enables
 *       mutual exclusion on each access path. Without it, misuse may only be detected
 *       in debug builds via runtime guards.
 *
 * @warning Returning references or pointers into the provided Buffer via the
 *          'present()' or 'acquire()' methods is undefined behavior.
 * @warning Exceptions thrown by callables provided to 'present()' or 'acquire()' are
 *          not handled internally. See documentation of methods for exact effects.
 */
template <class Buffer>
class TripleBuffer {
	struct alignas(HDIS) TripleBufferContext {
		using AtomBuf = std::atomic<Buffer*>;

		alignas(HDIS) Buffer m_writer_buffer;
		alignas(HDIS) Buffer m_reader_buffer;
		alignas(HDIS) Buffer m_middle_buffer;

#ifdef TRIPLE_BUFFER_ENFORCE_SPSC
		alignas(HDIS) mutable std::mutex m_reader_lock;
		mutable std::atomic_size_t m_present_count{};
		alignas(HDIS) /*****/ std::mutex m_writer_lock;
		/*****/ std::atomic_size_t m_acquire_count{};
#else
		alignas(HDIS)
	#if !defined(NDEBUG) || defined(DEBUG)
		mutable std::atomic_flag m_reader_used{};
		/*****/ std::atomic_flag m_writer_used{};
	#endif
		mutable std::atomic_size_t m_present_count{};
		/*****/ std::atomic_size_t m_acquire_count{};
#endif

		/*
		 * Synchronization model - single atomic handoff via m_swap_ptr
		 * ============================================================
		 *
		 * At all times, ownership of the three Buffer slots is partitioned as follows:
		 *
		 *   m_work_ptr - exclusively owned by the writer thread
		 *   m_read_ptr - exclusively owned by the reader thread
		 *   m_swap_ptr - the shared handoff slot; owned by neither thread exclusively
		 *
		 * The LSB of 'm_swap_ptr' is used as a dirty flag (pointer tagging), indicating
		 * that the writer has published a new buffer that the reader has not yet consumed.
		 *
		 * Writer path (acquire):
		 *   1. Writes freely into '*m_work_ptr' (exclusive ownership, no sync needed)
		 *   2. Publishes via: `m_work_ptr = sub_dirty(m_swap_ptr.exchange(
		 *                          add_dirty(m_work_ptr), acq_rel))`
		 *      - The 'release' side ensures all writes to '*m_work_ptr' are visible
		 *        before the pointer is installed into 'm_swap_ptr'.
		 *      - The returned pointer (the previous swap slot) becomes the new writer
		 *        buffer; 'sub_dirty()' strips any residual dirty tag left by a prior cycle.
		 *
		 * Reader path (present):
		 *   1. Checks `m_swap_ptr.load(acquire)` for the dirty flag.
		 *      - If clean, no exchange is performed; the reader continues with its
		 *        existing m_read_ptr at the cost of a single atomic load. This is the
		 *        fast path for a reader that is polling faster than the writer publishes.
		 *      - The 'acquire' ensures that if a dirty pointer is observed, all writes
		 *        the producer made before its 'release' exchange are now visible.
		 *   2. If dirty: swaps 'm_read_ptr' in via `m_swap_ptr.exchange(m_read_ptr, acq_rel)`,
		 *      taking ownership of the freshly published buffer.
		 *   3. Reads freely from '*m_read_ptr' (exclusive ownership after swap, no sync needed)
		 *
		 * Ownership invariant:
		 *   Each buffer slot is pointed to by exactly one of {m_work_ptr, m_read_ptr,
		 *   m_swap_ptr} at any given time. The exchange operations are the only moments
		 *   where a slot transitions between owners, and 'acq_rel' on both sides ensures
		 *   a happens-before edge is established across the producer/consumer boundary.
		 *
		 * Consequence for 'm_read_ptr' and 'm_work_ptr':
		 *   Neither pointer is shared between threads. 'm_work_ptr' is writer-only,
		 *   'm_read_ptr' is reader-only. Plain (non-atomic) reads/writes to both are
		 *   safe under the SPSC guarantee.
		 */

		alignas(HDIS) /*****/ Buffer* m_work_ptr = &m_writer_buffer;
		alignas(HDIS) mutable Buffer* m_read_ptr = &m_reader_buffer;
		alignas(HDIS) mutable AtomBuf m_swap_ptr = &m_middle_buffer;

		template <typename... Args>
		TripleBufferContext(Args&&... args) noexcept(std::is_nothrow_constructible_v<Buffer, Args...>)
			: m_writer_buffer(std::forward<Args>(args)...)
			, m_reader_buffer(std::forward<Args>(args)...)
			, m_middle_buffer(std::forward<Args>(args)...)
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
		requires std::constructible_from<Buffer, Args...>
	TripleBuffer(Args&&... args) noexcept(std::is_nothrow_constructible_v<Buffer, Args...>)
		: m_context(std::make_unique<TripleBufferContext>(std::forward<Args>(args)...))
	{}

	auto get_present_count() const noexcept { return m_context->m_present_count.load(std::memory_order::relaxed); }
	auto get_acquire_count() const noexcept { return m_context->m_acquire_count.load(std::memory_order::relaxed); }


private:
	template <bool DIRTY>
	struct BufferView {
		static constexpr bool dirty = DIRTY;
		const Buffer& buffer;
	};

	class ActiveGuard {
		std::atomic_flag& m_flag;

	public:
		ActiveGuard(std::atomic_flag& flag) noexcept : m_flag(flag) {
			if (m_flag.test_and_set(std::memory_order::relaxed))
				[[unlikely]] { std::terminate(); }
		}

		~ActiveGuard() noexcept {
			m_flag.clear(std::memory_order::relaxed);
		}
	};


public:
	/**
	 * @brief Provides read-only Buffer access to a reader in a thread-safe, single-shot manner.
	 *
	 * The callable is invoked with a single 'BufferView<DIRTY>' argument,
	 * and exposes the following access members:
	 *
	 *   - dirty - static bool, marks whether buffer was updated (dirty) since last read.
	 *   - buffer - const reference to the underlying buffer object.
	 *
	 * Callables are recommended to accept the argument as 'auto' to handle both
	 * 'BufferView<true>' and 'BufferView<false>' cases transparently.
	 *
	 * @tparam Fn  A callable that can be invoked with 'auto' and may return
	 *             any type (including 'void').
	 *
	 * @param callable  The callable to invoke.
	 *
	 * @return decltype(auto) - forwards anything the callable returns.
	 * @warning In case of an exception thrown during invocation of the callable,
	 *          the 'present_count' is incremented and the reader Buffer is swapped.
	 *          If the Buffer was dirty, the dirty tag is also consumed. The callable
	 *          will not return a value, if it was meant to do so.
	 *
	 * Thread-safety: See notes from the class description.
	 */
	template <typename Fn> requires (
		std::is_invocable_v<Fn, BufferView<true>> &&
		std::is_invocable_v<Fn, BufferView<false>>
	)
	decltype(auto) present(Fn&& callable) const noexcept(
		std::is_nothrow_invocable_v<Fn, BufferView<true>> &&
		std::is_nothrow_invocable_v<Fn, BufferView<false>>
	) {
#ifdef TRIPLE_BUFFER_ENFORCE_SPSC
		std::scoped_lock lock(m_context->m_reader_lock);
#elif !defined(NDEBUG) || defined(DEBUG)
		ActiveGuard guard(m_context->m_reader_used);
#endif
		m_context->m_present_count.fetch_add(1, std::memory_order::relaxed);

		if (get_dirty(m_context->m_swap_ptr.load(std::memory_order::acquire))) {
			m_context->m_read_ptr = sub_dirty(m_context->m_swap_ptr.exchange(
				m_context->m_read_ptr, std::memory_order::acq_rel));

			return callable(BufferView<true>{ *m_context->m_read_ptr });
		} else {
			return callable(BufferView<false>{ *m_context->m_read_ptr });
		}
	}

	/**
	 * @brief Provides Buffer access to a writer in a thread-safe, single-shot manner.
	 *
	 * The callable is invoked with a single 'Buffer&' argument. The callable may
	 * freely modify the contents of this buffer and once completes, the buffer is
	 * safely published for consumption.
	 *
	 * Callables are recommended to accept the argument explicitly as 'auto&'.
	 *
	 * @tparam Fn  A callable that can be invoked with 'auto&' and may return
	 *             any type (including 'void').
	 *
	 * @param callable  The callable to invoke.
	 *
	 * @return decltype(auto) - forwards anything the callable returns.
	 * @warning In case of an exception thrown during invocation of the callable,
	 *          only the 'acquire_count' is incremented. No Buffer swap occurs,
	 *          nor does the callable return a value, if it was meant to do so.
	 *
	 * Thread-safety: See notes from the class description.
	 */
	template <typename Fn> requires (std::is_invocable_v<Fn, Buffer&>)
	decltype(auto) acquire(Fn&& callable) noexcept(
		std::is_nothrow_invocable_v<Fn, Buffer&>
	) {
#ifdef TRIPLE_BUFFER_ENFORCE_SPSC
		std::scoped_lock lock(m_context->m_writer_lock);
#elif !defined(NDEBUG) || defined(DEBUG)
		ActiveGuard guard(m_context->m_writer_used);
#endif
		m_context->m_acquire_count.fetch_add(1, std::memory_order::relaxed);

		if constexpr (std::is_void_v<std::invoke_result_t<Fn, Buffer&>>) {
			callable(*m_context->m_work_ptr);
			m_context->m_work_ptr = sub_dirty(m_context->m_swap_ptr.exchange(
				add_dirty(m_context->m_work_ptr), std::memory_order::acq_rel));
		}
		else {
			decltype(auto) result = callable(*m_context->m_work_ptr);
			m_context->m_work_ptr = sub_dirty(m_context->m_swap_ptr.exchange(
				add_dirty(m_context->m_work_ptr), std::memory_order::acq_rel));
			return result;
		}
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
