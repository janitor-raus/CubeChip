/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <span>
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "HDIS_HCIS.hpp"
#include "ExecPolicy.hpp"

/*==================================================================*/

#ifndef MAX_ALIGN
	#define MAX_ALIGN HDIS * 2
#endif

/*==================================================================*/

template<typename T>
concept Allocatable = std::is_object_v<T> && !std::is_abstract_v<T>;

/**
 * @brief Free-standing heap Deleter for non-trivially destructible aligned memory.
 * @tparam T :: The type of data to destroy and deallocate.
 * @tparam A :: The alignment offset (optional). Must be a power of two.
 * @param[in] size :: The amount of elements this Deleter is managing.
 */
template <Allocatable T, std::size_t A>
class AlignedTypeArrayDeleter {
	static_assert((A & (A - 1)) == 0,
		"N must be a power of two.");
	static_assert(A <= MAX_ALIGN,
		"Exceeded maximum allowed alignment.");
	static_assert(!std::is_trivially_destructible_v<T>,
		"Deleter expects non-trivially-destructible types.");

	std::size_t mSize{};

public:
	constexpr AlignedTypeArrayDeleter() noexcept = default;
	constexpr AlignedTypeArrayDeleter(std::size_t size) noexcept : mSize{ size } {}

	void operator()(T* ptr) const
		noexcept(std::is_nothrow_destructible_v<T>)
	{
		if (ptr) { std::destroy_n(EXEC_POLICY(unseq) ptr, mSize); }
		::operator delete[](ptr, std::align_val_t(A));
	}

	friend constexpr void swap(AlignedTypeArrayDeleter& lhs, AlignedTypeArrayDeleter& rhs) noexcept
		{ std::swap(lhs.mSize, rhs.mSize); }
};

/**
 * @brief Free-standing heap Deleter for trivially destructible aligned memory.
 * @tparam T :: The type of data to destroy and deallocate.
 * @tparam A :: The alignment offset (optional). Must be a power of two.
 */
template <Allocatable T, std::size_t A>
class AlignedLiteArrayDeleter {
	static_assert((A & (A - 1)) == 0,
		"N must be a power of two.");
	static_assert(A <= MAX_ALIGN,
		"Exceeded maximum allowed alignment.");
	static_assert(std::is_trivially_destructible_v<T>,
		"Deleter expects trivially-destructible types.");

public:
	constexpr AlignedLiteArrayDeleter() noexcept = default;
	constexpr AlignedLiteArrayDeleter(std::size_t) noexcept {};

	void operator()(T* ptr) const noexcept
		{ ::operator delete[](ptr, std::align_val_t(A)); }

	friend constexpr void swap(AlignedLiteArrayDeleter&, AlignedLiteArrayDeleter&) noexcept {}
};

/*==================================================================*/

/**
 * @brief Free-standing unique_ptr type for aligned memory.
 * @tparam T :: The type of data to manage.
 * @tparam A :: The alignment offset (optional). Must be a power of two.
 */
template <Allocatable T, std::size_t A = HDIS>
using AlignedUniqueArray = std::unique_ptr<T[], std::conditional_t<
	std::is_trivially_destructible_v<T>,
	AlignedLiteArrayDeleter<T, A>,
	AlignedTypeArrayDeleter<T, A>
>>;

template <Allocatable T, std::size_t A>
class AlignedMemoryBlock;

template <Allocatable T, std::size_t A>
AlignedMemoryBlock<T, A> allocate_n(std::size_t) noexcept;

/*==================================================================*/

template <Allocatable T, std::size_t A>
class AlignedContainer {
	using memory_type = AlignedUniqueArray<T, A>;
	using self = AlignedContainer;

public:
	using element_type    = T;
	using size_type       = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type      = std::remove_cv_t<T>;

	using pointer       = T*;
	using const_pointer = const T*;

	using reference       = T&;
	using const_reference = const T&;

	using iterator       = T*;
	using const_iterator = const T*;

	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
	/* */ memory_type pData;
	const size_type   mSize;

public:
	constexpr size_type size()       const noexcept { return mSize; }
	constexpr size_type size_bytes() const noexcept { return size() * sizeof(T); }
	constexpr bool      empty()      const noexcept { return size() == 0; }
	constexpr auto      span()       const noexcept { return std::span(data(), size()); }

	constexpr pointer   data()  { return pData.get(); }
	constexpr reference front() { return data()[0]; }
	constexpr reference back()  { return data()[size() - 1]; }

	constexpr const_pointer   data()  const { return pData.get(); }
	constexpr const_reference front() const { return data()[0]; }
	constexpr const_reference back()  const { return data()[size() - 1]; }

	explicit AlignedContainer(memory_type&& memory_block, size_type size) noexcept
		: pData{ std::move(memory_block) }, mSize{ size } {}

public:
	constexpr reference at(size_type idx) {
		if (idx >= size()) { throw std::out_of_range("AlignedContainer.at() index out of range"); }
		return data()[idx];
	}
	constexpr const_reference at(size_type idx) const {
		if (idx >= size()) { throw std::out_of_range("AlignedContainer.at() index out of range"); }
		return data()[idx];
	}

	constexpr reference operator()(size_type idx) {
		assert(idx < size() && "AlignedContainer.operator() index out of bounds");
		return data()[idx];
	}
	constexpr reference operator[](size_type idx) {
		assert(idx < size() && "AlignedContainer.operator[] index out of bounds");
		return data()[idx];
	}

	constexpr const_reference operator()(size_type idx) const {
		assert(idx < size() && "AlignedContainer.operator() index out of bounds");
		return data()[idx];
	}
	constexpr const_reference operator[](size_type idx) const {
		assert(idx < size() && "AlignedContainer.operator[] index out of bounds");
		return data()[idx];
	}

public:
	constexpr iterator begin() noexcept { return data(); }
	constexpr iterator end()   noexcept { return data() + size(); }
	constexpr reverse_iterator rbegin() noexcept { return std::make_reverse_iterator(end()); }
	constexpr reverse_iterator rend()   noexcept { return std::make_reverse_iterator(begin()); }

	constexpr const_iterator begin() const noexcept { return data(); }
	constexpr const_iterator end()   const noexcept { return data() + size(); }
	constexpr const_reverse_iterator rbegin() const noexcept { return std::make_reverse_iterator(end()); }
	constexpr const_reverse_iterator rend()   const noexcept { return std::make_reverse_iterator(begin()); }

	constexpr const_iterator cbegin() const noexcept { return begin(); }
	constexpr const_iterator cend()   const noexcept { return end(); }
	constexpr const_reverse_iterator crbegin() const noexcept { return std::make_reverse_iterator(cend()); }
	constexpr const_reverse_iterator crend()   const noexcept { return std::make_reverse_iterator(cbegin()); }
};

/*==================================================================*/

template <Allocatable T, std::size_t A>
class AlignedMemoryBlock {
	using memory_type = AlignedUniqueArray<T, A>;
	using self = AlignedMemoryBlock;
	using size_type = std::size_t;

	memory_type mAllocated;
	size_type mSize{};
	size_type mOffset{};

private:
	friend self allocate_n<T, A>(std::size_t) noexcept;

	AlignedMemoryBlock(T* ptr, size_type size) noexcept
		: mAllocated{ ptr, { size } }, mSize{ size }
	{}

	constexpr size_type clamp_element_construction_count(size_type count) const noexcept
		{ return count ? std::min(count, remaining_count()) : remaining_count(); }

public:
	AlignedMemoryBlock(const self&) = delete;
	AlignedMemoryBlock(self&&)      = default;
	self& operator=(const self&) = delete;
	self& operator=(self&&)      = default;

public:
	constexpr size_type element_count()   const noexcept { return mSize; }
	constexpr size_type construct_count() const noexcept { return mOffset; }
	constexpr size_type remaining_count() const noexcept { return element_count() - construct_count(); }

public:
	constexpr bool is_constructed() const noexcept { return construct_count() >= element_count(); }
	constexpr bool has_valid_ptr()  const noexcept { return mAllocated.get() != nullptr; }

	[[nodiscard]] 
	memory_type release() noexcept
		{ return has_valid_ptr() ? std::move(mAllocated) : memory_type{}; }

	[[nodiscard]] 
	memory_type release_if_constructed() noexcept
		{ return is_constructed() ? std::move(mAllocated) : memory_type{}; }
	
	[[nodiscard]] 
	AlignedContainer<T, A> release_as_container() noexcept {
		return AlignedContainer<T, A>(has_valid_ptr() \
			? std::move(mAllocated) : memory_type{}, element_count());
	}

	[[nodiscard]] 
	AlignedContainer<T, A> release_as_container_if_constructed() noexcept {
		return AlignedContainer<T, A>(is_constructed() \
			? std::move(mAllocated) : memory_type{}, element_count());
	}

	/*==================================================================*/

public:
	[[nodiscard]] self& as_value(size_type count = 0u) &
		noexcept(std::is_nothrow_constructible_v<T>)
		requires(std::is_default_constructible_v<T>)
	{
		if (has_valid_ptr() && !is_constructed()) {
			const auto safe_count{ clamp_element_construction_count(count) };
			std::uninitialized_value_construct_n(EXEC_POLICY(unseq)
				mAllocated.get() + construct_count(), safe_count);
			mOffset += safe_count;
		}
		return *this;
	}

	[[nodiscard]] self as_value(size_type count = 0u) &&
		noexcept(std::is_nothrow_constructible_v<T>)
		requires(std::is_default_constructible_v<T>)
	{
		return std::move(this->as_value(count));
	}

	[[nodiscard]] self& as_default(size_type count = 0u) &
		noexcept(std::is_nothrow_default_constructible_v<T>)
		requires(std::is_default_constructible_v<T>)
	{
		if (has_valid_ptr() && !is_constructed()) {
			const auto safe_count{ clamp_element_construction_count(count) };
			std::uninitialized_default_construct_n(EXEC_POLICY(unseq)
				mAllocated.get() + construct_count(), safe_count);
			mOffset += safe_count;
		}
		return *this;
	}

	[[nodiscard]] self as_default(size_type count = 0u) &&
		noexcept(std::is_nothrow_default_constructible_v<T>)
		requires(std::is_default_constructible_v<T>)
	{
		return std::move(this->as_default(count));
	}

	template <typename V>
	[[nodiscard]] self& by_fill(V&& value, size_type count = 0u) &
		noexcept(std::is_nothrow_copy_constructible_v<T>)
		requires(std::is_copy_constructible_v<T> && std::is_convertible_v<V&&, T>)
	{
		if (has_valid_ptr() && !is_constructed()) {
			const auto safe_count{ clamp_element_construction_count(count) };
			std::uninitialized_fill_n(EXEC_POLICY(unseq)
				mAllocated.get() + construct_count(), safe_count, std::forward<V>(value));
			mOffset += safe_count;
		}
		return *this;
	}

	template <typename V>
	[[nodiscard]] self by_fill(V&& value, size_type count = 0u) &&
		noexcept(std::is_nothrow_copy_constructible_v<T>)
		requires(std::is_copy_constructible_v<T> && std::is_convertible_v<V&&, T>)
	{
		return std::move(this->by_fill(std::forward<V>(value), count));
	}

	// * No range overlap protection for trivial T=V types in fast path.
	template <typename V>
	[[nodiscard]] self& by_copy(const V* from, size_type count = 0u) &
		noexcept(std::is_nothrow_copy_constructible_v<T>)
		requires(std::is_copy_constructible_v<T> && std::is_convertible_v<V, T>)
	{
		if (has_valid_ptr() && !is_constructed()) {
			const auto safe_count{ clamp_element_construction_count(count) };
			if constexpr (std::is_same_v<T, V> && std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>) {
				std::memcpy(mAllocated.get() + construct_count(), from, safe_count * sizeof(T));
			} else {
				std::uninitialized_copy_n(EXEC_POLICY(unseq)
					from, safe_count, mAllocated.get() + construct_count());
			}
			mOffset += safe_count;
		}
		return *this;
	}

	// * No range overlap protection for trivial T=V types in fast path.
	template <typename V>
	[[nodiscard]] self by_copy(const V* from, size_type count = 0u) &&
		noexcept(std::is_nothrow_copy_constructible_v<T>)
		requires(std::is_copy_constructible_v<T> && std::is_convertible_v<V, T>)
	{
		return std::move(this->by_copy(from, count));
	}

	template <typename V>
	[[nodiscard]] self& by_move(V* from, size_type count = 0u) &
		noexcept(std::is_nothrow_move_constructible_v<T>)
		requires(std::is_move_constructible_v<T> && std::is_convertible_v<V, T>)
	{
		if (has_valid_ptr() && !is_constructed()) {
			const auto safe_count{ clamp_element_construction_count(count) };
			if constexpr (std::is_same_v<T, V> && std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>) {
				std::memmove(mAllocated.get() + construct_count(), from, safe_count * sizeof(T));
			} else {
				std::uninitialized_move_n(EXEC_POLICY(unseq)
					from, safe_count, mAllocated.get() + construct_count());
			}
			mOffset += safe_count;
		}
		return *this;
	}

	template <typename V>
	[[nodiscard]] self by_move(V* from, size_type count = 0u) &&
		noexcept(std::is_nothrow_move_constructible_v<T>)
		requires(std::is_move_constructible_v<T> && std::is_convertible_v<V, T>)
	{
		return std::move(this->by_move(from, count));
	}
};

/*==================================================================*/

template<Allocatable T, std::size_t A = HDIS>
inline AlignedMemoryBlock<T, A> allocate_n(std::size_t size) noexcept {
	static_assert((A & (A - 1)) == 0u,
		"N must be a power of two.");
	static_assert(A <= MAX_ALIGN,
		"Exceeded maximum allowed alignment.");

	void* ptr{ size ? ::operator new[](size * sizeof(T), std::align_val_t(A), std::nothrow) : nullptr };
	return { static_cast<T*>(ptr), { ptr != nullptr ? size : 0 } };
}

/*==================================================================*/

template <Allocatable T, std::size_t A>
struct AlignedDeleter {
	static_assert((A & (A - 1)) == 0,
		"N must be power of two.");
	static_assert(A <= MAX_ALIGN,
		"Exceeded maximum alignment.");

	void operator()(T* ptr) const noexcept {
		if (ptr) { ptr->~T(); }
		::operator delete(ptr, std::align_val_t(A));
	}
};

template <Allocatable T, std::size_t A = HDIS>
using AlignedUnique = std::unique_ptr<T, AlignedDeleter<T, A>>;

template <Allocatable T, std::size_t A = HDIS, typename... Args>
AlignedUnique<T, A> allocate(Args&&... args) noexcept {
	return { new (std::align_val_t(A), std::nothrow) T(std::forward<Args>(args)...), {} };
}

/*==================================================================*/

#ifdef MAX_ALIGN
	#undef MAX_ALIGN
#endif
