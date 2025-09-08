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
#include "AtomSharedPtr.hpp"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4324)
#endif

/*==================================================================*/

/**
 * @brief Thread-safe triple-buffer for concurrent reading and writing.
 *
 * @details
 * TripleBuffer maintains three independent buffers:
 * - **Work buffer**: receives writes from the producer.
 * - **Read buffer**: supplies reads to the consumer.
 * - **Swap buffer**: used internally to exchange data between work and read.
 *
 * Reads and writes are single-call operations that do not require manual
 * lock management or buffer reservation. The buffer ensures that a full read
 * or write operation completes without partial state exposure.
 *
 * All public methods that modify or read buffer contents acquire the
 * appropriate shared or exclusive lock internally:
 * - Read methods (`read()`, `copy()`) acquire a shared lock on the read buffer.
 * - Write methods (`write()`) acquire an exclusive lock on the work buffer.
 * - Resizing the buffer (`resize()`) acquires exclusive locks on both.
 *
 * @tparam T1 Element type stored in the buffer. Must be trivially copyable.
 */
template <typename T1>
	requires (std::is_trivially_copyable_v<T1>)
class alignas(HDIS) TripleBuffer {
	using value_type = T1;
	using Buffer     = AlignedUniqueArray<T1>;
	using AtomBuf    = Atom<Buffer*>;
	using size_type  = std::size_t;

private:
	Buffer mWorkBuffer;
	Buffer mReadBuffer;
	Buffer mSwapBuffer;

	mutable std::shared_mutex mReadLock;
	mutable std::shared_mutex mWorkLock;
	size_type mSize{};

	alignas(HDIS) mutable Buffer* mpWork{ &mWorkBuffer };
	alignas(HDIS) mutable Buffer* mpRead{ &mReadBuffer };
	alignas(HDIS) mutable AtomBuf mpSwap{ &mSwapBuffer };

private:
	static constexpr std::uintptr_t sNewDataFlag{ 1 };

	constexpr bool getFlag(Buffer* ptr) const noexcept {
		return reinterpret_cast<std::uintptr_t>(ptr) & sNewDataFlag;
	}

	constexpr Buffer* addFlag(Buffer* ptr) const noexcept {
		auto new_ptr{ reinterpret_cast<std::uintptr_t>(ptr) };
		return reinterpret_cast<Buffer*>(new_ptr | sNewDataFlag);
	}

	constexpr Buffer* subFlag(Buffer* ptr) const noexcept {
		auto new_ptr{ reinterpret_cast<std::uintptr_t>(ptr) };
		return reinterpret_cast<Buffer*>(new_ptr & ~sNewDataFlag);
	}

/*==================================================================*/

public:
	TripleBuffer(size_type buffer_size) noexcept
		: mWorkBuffer{ ::allocate_n<T1>(buffer_size).as_value().release() }
		, mReadBuffer{ ::allocate_n<T1>(buffer_size).as_value().release() }
		, mSwapBuffer{ ::allocate_n<T1>(buffer_size).as_value().release() }
		, mSize{ mWorkBuffer && mReadBuffer && mSwapBuffer ? buffer_size : 0 }
	{}

	TripleBuffer() noexcept : TripleBuffer(0) {} // XXX temporary default constructor

	TripleBuffer(const TripleBuffer&) = delete;
	TripleBuffer(TripleBuffer&&)      = delete;
	TripleBuffer& operator=(const TripleBuffer&) = delete;
	TripleBuffer& operator=(TripleBuffer&&)      = delete;

	auto size() const noexcept { return mSize; }

	/**
	 * @brief Resizes all internal buffers of the TripleBuffer to the specified size.
	 *
	 * @param buffer_size The new size (number of elements) for each buffer.
	 *
	 * @note Acquires exclusive locks for both read and work buffers.
	 */
	void resize(size_type buffer_size) noexcept {
		std::unique_lock read_lock{ mReadLock };
		std::unique_lock work_lock{ mWorkLock };

		mWorkBuffer.reset(); mWorkBuffer = ::allocate_n<T1>(buffer_size).as_value().release();
		mReadBuffer.reset(); mReadBuffer = ::allocate_n<T1>(buffer_size).as_value().release();
		mSwapBuffer.reset(); mSwapBuffer = ::allocate_n<T1>(buffer_size).as_value().release();
		mSize = mWorkBuffer && mReadBuffer && mSwapBuffer ? buffer_size : 0;
		mpWork = &mWorkBuffer;
		mpRead = &mReadBuffer;
		mpSwap = &mSwapBuffer;
	}

	/*==================================================================*/

private:
	auto clamp_count(size_type count) const noexcept {
		const auto limit{ size() };
		return count ? std::min(count, limit) : limit;
	}


private:
	T1* acquireReadBuffer() const noexcept {
		if (getFlag(mpSwap.load(mo::acquire))) {
			mpRead = subFlag(mpSwap.exchange(mpRead, mo::acq_rel));
		}
		return mpRead->get();
	}

public:
	/**
	 * @brief Copies contents from the TripleBuffer into a heap-allocated container.
	 *
	 * @param count Size of returned container; if zero, matches the size of the buffer.
	 * May be larger than the buffer size, extra elements will be value-initialized.
	 * @return A heap-allocated container for the requested contents.
	 * 
	 * @note Acquires a shared lock on the read buffer.
	 */
	[[nodiscard]]
	auto copy(size_type count = 0u) noexcept {
		std::shared_lock lock{ mReadLock };
		const auto new_count{ clamp_count(count) };
		return ::allocate_n<T1>(count ? count : size()) \
			.by_copy(acquireReadBuffer(), new_count).as_value() \
			.release_as_container_if_constructed();
	}

	/**
	 * @brief Copies contents from the TripleBuffer into the provided output array.
	 *
	 * @tparam T2 Type of the output elements; must be copy-constructible and convertible from T1.
	 * @param output Pointer to the output array where elements will be copied.
	 * Maximum amount of copied elements is determined by the buffer size.
	 * @param count Number of elements to read; if zero, reads the entire buffer.
	 *
	 * @note Acquires a shared lock on the read buffer.
	 */
	template <typename T2>
	void read(T2* output, size_type count = 0) const
		noexcept(std::is_nothrow_convertible_v<T1, T2>)
		requires(std::is_trivially_copyable_v<T2> && std::is_convertible_v<T1, T2>)
	{
		std::shared_lock lock{ mReadLock };
		std::copy_n(EXEC_POLICY(unseq)
			acquireReadBuffer(), clamp_count(count), output);
	}

	/**
	 * @brief Copies contents from the TripleBuffer into a contiguous container.
	 *
	 * @tparam T2 A contiguous container type with value_type convertible to T1.
	 * @param output The container to copy data into.
	 * 
	 * @note Acquires a shared lock on the read buffer.
	 */
	template <IsContiguousContainer T2>
	void read(T2& output) const
		noexcept(std::is_nothrow_convertible_v<T1, ValueType<T2>>)
		requires(std::is_trivially_copyable_v<ValueType<T2>> && std::is_convertible_v<T1, ValueType<T2>>)
	{
		std::shared_lock lock{ mReadLock };
		std::copy_n(EXEC_POLICY(unseq)
			acquireReadBuffer(), clamp_count(std::size(output)), std::data(output));
	}

	/*==================================================================*/

private:
	void commitWorkerChanges() noexcept {
		mpWork = subFlag(mpSwap.exchange(addFlag(mpWork), mo::acq_rel));
	}

public:
	/**
	 * @brief Writes to the TripleBuffer by transforming and copying data from a raw pointer.
	 *
	 * @tparam T2 Type of the input data elements.
	 * @tparam Lambda Unary function applied to each input element before writing.
	 * @param data Pointer to the source data.
	 * @param count Number of elements to write. Maximum is clamped to the buffer size.
	 * @param function Transformation function applied to each element before storing.
	 *
	 * @note Acquires an exclusive lock on the work buffer.
	 */
	template <typename T2, typename Lambda>
	void write(const T2* data, size_type count, Lambda&& function)
		noexcept(std::is_nothrow_convertible_v<T2, T1>)
		requires(std::is_trivially_copyable_v<T2>&& std::is_convertible_v<T2, T1>)
	{
		std::unique_lock lock{ mWorkLock };
		std::transform(EXEC_POLICY(unseq)
			data, data + clamp_count(count), mpWork->get(), function);

		commitWorkerChanges();
	}

	/**
	 * @brief Writes to the TripleBuffer by copying data from a contiguous container.
	 *
	 * @tparam T2 A contiguous container type whose element size matches T1 and is trivially copyable.
	 * @param data The container whose contents will be copied into the buffer.
	 *
	 * @note Acquires an exclusive lock on the work buffer.
	 */
	template <IsContiguousContainer T2>
	void write(const T2& data)
		noexcept(std::is_nothrow_convertible_v<ValueType<T2>, T1>)
		requires(std::is_trivially_copyable_v<ValueType<T2>>&& std::is_convertible_v<ValueType<T2>, T1>)
	{
		std::unique_lock lock{ mWorkLock };
		std::copy(EXEC_POLICY(unseq)
			std::begin(data), std::end(data), mpWork->get());

		commitWorkerChanges();
	}

	/**
	 * @brief Writes to the TripleBuffer by applying a unary transformation to each element of a contiguous container.
	 *
	 * @tparam T2 A contiguous container type.
	 * @tparam Lambda Unary function applied to each element before writing.
	 * @param data The container whose elements will be transformed and written.
	 * @param function Transformation function applied to each element before storing.
	 *
	 * @note Acquires an exclusive lock on the work buffer.
	 */
	template <IsContiguousContainer T2, typename Lambda>
	void write(const T2& data, Lambda&& function)
		noexcept(std::is_nothrow_convertible_v<ValueType<T2>, T1>)
		requires(std::is_trivially_copyable_v<ValueType<T2>>&& std::is_convertible_v<ValueType<T2>, T1>)
	{
		std::unique_lock lock{ mWorkLock };
		std::transform(EXEC_POLICY(unseq)
			std::begin(data), std::end(data), mpWork->get(), function);

		commitWorkerChanges();
	}

	/**
	 * @brief Writes to the TripleBuffer by applying a binary transformation to elements from two contiguous containers.
	 *
	 * @tparam T2 A contiguous container type.
	 * @tparam Lambda Binary function applied to corresponding elements from both containers before writing.
	 * @param data1 The first input container.
	 * @param data2 The second input container.
	 * @param function Transformation function applied to each pair of elements before storing.
	 *
	 * @note Acquires an exclusive lock on the work buffer.
	 */
	template <IsContiguousContainer T2, typename Lambda>
	void write(const T2& data1, const T2& data2, Lambda&& function)
		noexcept(std::is_nothrow_convertible_v<ValueType<T2>, T1>)
		requires(std::is_trivially_copyable_v<ValueType<T2>>&& std::is_convertible_v<ValueType<T2>, T1>)
	{
		std::unique_lock lock{ mWorkLock };
		std::transform(EXEC_POLICY(unseq)
			std::begin(data1), std::end(data1), std::begin(data2), mpWork->get(), function);

		commitWorkerChanges();
	}
};

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
