/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <span>
#include <array>
#include <vector>
#include <cstddef>
#include <stdexcept>

#include "Concepts.hpp"

/*==================================================================*/

#pragma region RangeProxy Class
template <typename T>
struct RangeProxy final {
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

protected:
	pointer   mData;
	size_type mSize;

public:
	constexpr RangeProxy(pointer data, size_type length) noexcept
		: mData { data  }, mSize{ length }
	{}

	template <std::size_t N >
	constexpr RangeProxy(T(&array)[N]) noexcept
		: mData{ array }, mSize{ N }
	{}

	template <IsContiguousContainer Object>
		requires (!std::is_rvalue_reference_v<Object>)
	constexpr RangeProxy(const Object& array) noexcept
		: mData{ const_cast<pointer>(std::data(array)) }, mSize{ std::size(array) }
	{}

	constexpr       pointer data()       noexcept { return mData; }
	constexpr const_pointer data() const noexcept { return mData; }

	constexpr size_type size()       const noexcept { return mSize; }
	constexpr size_type size_bytes() const noexcept { return size() * sizeof(element_type); }
	constexpr bool      empty()      const noexcept { return size() == 0; }

	constexpr void reseat(pointer data)   noexcept { mData = data; }
	constexpr void resize(size_type size) noexcept { mSize = size; }

	constexpr       reference front()       noexcept { return data()[0]; }
	constexpr const_reference front() const noexcept { return data()[0]; }
	constexpr       reference back()        noexcept { return data()[size() - 1]; }
	constexpr const_reference back()  const noexcept { return data()[size() - 1]; }

	constexpr auto first(size_type count) const { return RangeProxy(data(), count); }
	constexpr auto last (size_type count) const { return RangeProxy(data() + size() - count, count); }

	constexpr       reference operator[](size_type idx)       { return data()[idx]; }
	constexpr const_reference operator[](size_type idx) const { return data()[idx]; }

	constexpr       reference at(size_type idx)       {
		if (idx < size()) { return data()[idx]; }
		else { throw std::out_of_range("RangeProxy.at() :: Index out of range."); }
	}
	constexpr const_reference at(size_type idx) const {
		if (idx < size()) { return data()[idx]; }
		else { throw std::out_of_range("RangeProxy.at() :: Index out of range."); }
	}

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

template <typename T, std::size_t N>
RangeProxy(std::array<T, N>) -> RangeProxy<T>;

template <typename T, std::size_t N = std::dynamic_extent>
RangeProxy(std::span<T, N>) -> RangeProxy<T>;

template <typename T>
RangeProxy(std::vector<T>) -> RangeProxy<T>;
#pragma endregion

/*==================================================================*/

#pragma region RangeIterator Class
template <typename T>
struct RangeIterator final {
	using element_type    = T;
	using size_type       = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type      = std::remove_cv_t<RangeProxy<T>>;

	using iterator_category = std::contiguous_iterator_tag;

	using pointer        = RangeProxy<T>*;
	using const_pointer  = const RangeProxy<T>*;

	using reference       = RangeProxy<T>&;
	using const_reference = const RangeProxy<T>&;

protected:
	RangeProxy<T> mRange;

public:
	constexpr RangeIterator(T* begin, size_type length) noexcept
		: mRange{ begin, length }
	{}

	template <std::size_t N>
	explicit constexpr RangeIterator(T(&array)[N]) noexcept
		: mRange{ array, N }
	{}

	template <IsContiguousContainer Object>
		requires (!std::is_rvalue_reference_v<Object>)
	explicit constexpr RangeIterator(const Object& array) noexcept
		: mRange{ std::data(array), std::size(array) }
	{}

public:
	constexpr auto& operator* () const noexcept { return mRange; }
	constexpr auto* operator->() const noexcept { return &mRange; }

	constexpr auto& operator++() noexcept { mRange.reseat(mRange.data() + mRange.size()); return *this; }
	constexpr auto& operator--() noexcept { mRange.reseat(mRange.data() - mRange.size()); return *this; }

	constexpr auto& operator+=(difference_type rhs) noexcept { mRange.reseat(mRange.data() + rhs * mRange.size()); return *this; }
	constexpr auto& operator-=(difference_type rhs) noexcept { mRange.reseat(mRange.data() - rhs * mRange.size()); return *this; }

	constexpr auto  operator++(int) noexcept { auto tmp{ *this }; mRange.reseat(mRange.data() + mRange.size()); return tmp; }
	constexpr auto  operator--(int) noexcept { auto tmp{ *this }; mRange.reseat(mRange.data() - mRange.size()); return tmp; }

	constexpr auto  operator+ (difference_type rhs) const noexcept { return RangeIterator(mRange.data() + rhs * mRange.size(), mRange.size()); }
	constexpr auto  operator- (difference_type rhs) const noexcept { return RangeIterator(mRange.data() - rhs * mRange.size(), mRange.size()); }

	constexpr friend auto operator+(difference_type lhs, const RangeIterator& rhs) noexcept { return rhs + lhs; }
	constexpr friend auto operator-(difference_type lhs, const RangeIterator& rhs) noexcept { return rhs - lhs; }

	constexpr difference_type operator-(const RangeIterator& other) const noexcept { return mRange.data() - other.mRange.data(); }

	constexpr bool operator==(const RangeIterator& other) const noexcept { return mRange.data() == other.mRange.data(); }
	constexpr bool operator!=(const RangeIterator& other) const noexcept { return mRange.data() != other.mRange.data(); }
	constexpr bool operator< (const RangeIterator& other) const noexcept { return mRange.data() <  other.mRange.data(); }
	constexpr bool operator> (const RangeIterator& other) const noexcept { return mRange.data() >  other.mRange.data(); }
	constexpr bool operator<=(const RangeIterator& other) const noexcept { return mRange.data() <= other.mRange.data(); }
	constexpr bool operator>=(const RangeIterator& other) const noexcept { return mRange.data() >= other.mRange.data(); }

	constexpr auto operator[](difference_type rhs) const noexcept { return RangeProxy<T>(mRange.data() + rhs * mRange.size(), mRange.size()); }
};

template <typename T, std::size_t N>
RangeIterator(std::array<T, N>) -> RangeIterator<T>;

template <typename T, std::size_t N = std::dynamic_extent>
RangeIterator(std::span<T, N>) -> RangeIterator<T>;

template <typename T>
RangeIterator(std::vector<T>) -> RangeIterator<T>;
#pragma endregion

/*==================================================================*/

#pragma region RangeProxy2D Class
template <typename T>
struct RangeProxy2D final {
	using element_type    = T;
	using axis_size       = std::uint32_t;
	using size_type       = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type      = RangeProxy<T>;

	using iterator       = RangeIterator<T>;
	using const_iterator = const RangeIterator<T>;

	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

protected:
	element_type* mData;
	axis_size mCols;
	axis_size mRows;

public:
	constexpr RangeProxy2D(element_type* data, size_type cols, size_type rows) noexcept
		: mData{ data }
		, mCols{ static_cast<axis_size>(cols) }
		, mRows{ static_cast<axis_size>(rows) }
	{}

	constexpr       element_type* data()         noexcept { return mData; }
	constexpr const element_type* data()   const noexcept { return mData; }

	constexpr size_type size()       const noexcept { return mCols * mRows; }
	constexpr size_type size_bytes() const noexcept { return size() * sizeof(element_type); }
	constexpr bool      empty()      const noexcept { return size() == 0; }

	constexpr size_type  lenX() const noexcept { return mCols; }
	constexpr size_type  lenY() const noexcept { return mRows; }

	constexpr value_type front() const noexcept { return value_type(data(), lenX()); }
	constexpr value_type back()  const noexcept { return value_type(data() + (lenY() - 1) * lenX()); }

	constexpr value_type first(size_type count) const { return value_type(data(), count); }
	constexpr value_type last (size_type count) const { return value_type(data() + size() - count, count); }

	constexpr value_type operator[](size_type idx) const { return value_type(data() + idx * lenX(), lenX()); }
	constexpr value_type at(size_type idx) const {
		if (idx < lenY()) { return value_type(data() + idx * lenX(), lenX()); }
		else { throw std::out_of_range("RangeProxy2D.at() :: Index out of range."); }
	}

	constexpr iterator begin() noexcept { return iterator(data(), lenX()); }
	constexpr iterator end()   noexcept { return iterator(data() + size(), lenX()); }
	constexpr reverse_iterator rbegin() noexcept { return std::make_reverse_iterator(end()); }
	constexpr reverse_iterator rend()   noexcept { return std::make_reverse_iterator(begin()); }

	constexpr const_iterator begin() const noexcept { return const_iterator(data(), lenX()); }
	constexpr const_iterator end()   const noexcept { return const_iterator(data() + size(), lenX()); }
	constexpr const_reverse_iterator rbegin() const noexcept { return std::make_reverse_iterator(end()); }
	constexpr const_reverse_iterator rend()   const noexcept { return std::make_reverse_iterator(begin()); }

	constexpr const_iterator cbegin() const noexcept { return const_iterator(data(), lenX()); }
	constexpr const_iterator cend()   const noexcept { return const_iterator(data() + size(), lenX()); }
	constexpr const_reverse_iterator crbegin() const noexcept { return std::make_reverse_iterator(cend()); }
	constexpr const_reverse_iterator crend()   const noexcept { return std::make_reverse_iterator(cbegin()); }
};
#pragma endregion
