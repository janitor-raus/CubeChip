/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <span>
#include <array>
#include <cmath>
#include <cassert>
#include <memory>
#include <algorithm>
#include <utility>

#include "RangeIterator.hpp"
#include "Aligned.hpp"
#include "HDIS_HCIS.hpp"
#include "ExecPolicy.hpp"

/*==================================================================*/

#ifndef MAX_ALIGN
	#define MAX_ALIGN HDIS * 2
#endif

/*==================================================================*/

template <typename T>
	requires (std::is_default_constructible_v<T>)
class Map2D final {
	using self = Map2D;

public:
	using element_type    = T;
	using axis_size       = std::uint32_t;
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
	axis_size mCols;
	axis_size mRows;
	AlignedUniqueArray<T> pData;

public:
	constexpr size_type size()       const noexcept { return mRows * mCols; }
	constexpr size_type size_bytes() const noexcept { return size() * sizeof(value_type); }
	constexpr bool      empty()      const noexcept { return size() == 0; }
	constexpr auto      span()       const noexcept { return std::span(data(), size()); }

	constexpr size_type lenX() const noexcept { return mCols; }
	constexpr size_type lenY() const noexcept { return mRows; }

	constexpr pointer   data()  { return pData.get(); }
	constexpr reference front() { return data()[0]; }
	constexpr reference back()  { return data()[size() - 1]; }

	constexpr const_pointer   data()  const { return pData.get(); }
	constexpr const_reference front() const { return data()[0]; }
	constexpr const_reference back()  const { return data()[size() - 1]; }

	constexpr auto first(size_type count) const { return RangeProxy(data(), count); }
	constexpr auto last (size_type count) const { return RangeProxy(data() + size() - count, count); }

	constexpr auto makeRowIter(size_type row)  const { return RangeIterator(data() + row * lenX(), lenX()); }
	constexpr auto makeRowProxy(size_type row) const { return *makeRowIter(row); }

	constexpr auto makeIter()  const { return RangeIterator<T>(data(), size()); }
	constexpr auto makeProxy() const { return *makeIter(); }

	constexpr auto makeProxy2D() const { return RangeProxy2D(data(), lenX(), lenY()); }

	#pragma region Trivial Ctor
	constexpr Map2D(size_type cols = 1u, size_type rows = 1u)
		: mCols{ std::max(1u, axis_size(cols)) }
		, mRows{ std::max(1u, axis_size(rows)) }
		, pData{ ::allocate_n<T>(size()).as_value().release() }
	{}
	#pragma endregion

	#pragma region Copy/Move Ctor
	constexpr Map2D(self&& other) noexcept
		: mCols{ std::exchange(other.mCols, 0) }
		, mRows{ std::exchange(other.mRows, 0) }
		, pData{ std::move(other.pData) }
	{}
	constexpr Map2D(const self& other) // copy constructor
		: Map2D{ other.mCols, other.mRows }
	{}
	#pragma endregion

	#pragma region Move/Copy Assignment
	constexpr self& operator=(self&& other) {
		pData = std::move(other.pData);
		mCols = std::exchange(other.mCols, 0);
		mRows = std::exchange(other.mRows, 0);
		return *this;
	}
	constexpr self& operator=(const self& other) { // copy assignment
		if (this != &other && size() == other.size()) {
			std::copy(EXEC_POLICY(unseq)
				other.begin(), other.end(), begin());

			mCols = other.mCols;
			mRows = other.mRows;
		}
		return *this;
	}
	#pragma endregion

public:
	#pragma region linearCopy() :: C++ Array
	/**
	 * @brief Copies data from another contiguous container. As much
	 *        as it can fit or pull.
	 * @returns Self reference for method chaining.
	 *
	 * @param[in] other :: Contiguous container to copy from.
	 */
	template <IsContiguousContainer Object>
	constexpr self& linearCopy(const Object& other) {
		std::copy_n(EXEC_POLICY(unseq)
			std::begin(other), std::min(size(), std::size(other)), begin());
		return *this;
	}
	#pragma endregion

	#pragma region linearCopy() :: C-style Array
	/**
	 * @brief Copies data from array pointer of the same type. As much
	 *        as it can fit or pull.
	 * @returns Self reference for method chaining.
	 *
	 * @param[in] other :: Pointer to array of data.
	 * @param[in] size  :: Amount of elements to copy (optional).
	 *
	 * @warning Cannot check if target array has sufficient data.
	 */
	template <size_type N>
	constexpr self& linearCopy(T(&other)[N], size_type len = N) {
		std::copy_n(EXEC_POLICY(unseq)
			other, std::min(len, size()), begin());
		return *this;
	}
	#pragma endregion

	#pragma region resize()
	/**
	 * @brief Resizes the matrix to new dimensions. Defaults to resizeDirty()
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows   :: Total rows of the new matrix.    (min: 1)
	 * @param[in] cols   :: Total columns of the new matrix. (min: 1)
	 *
	 * @warning A 'rows'/'cols' of 0 will default to current size.
	 */
	constexpr self& resize(size_type rows, size_type cols) {
		if (cols == lenX() && rows == lenY()) {
			return *this;
		} else {
			return resizeDirty(cols, rows);
		}
	}
	#pragma endregion

	#pragma region resizeDirty()
	constexpr self& resizeDirty(size_type cols, size_type rows) {
		const auto minCols{ std::min(cols, lenX()) };
		const auto minRows{ std::min(rows, lenY()) };

		auto pCopy{ ::allocate_n<T>(rows * cols).as_value().release() };
		if (!pCopy) { return *this; }

		for (size_type row{ 0u }; row < minRows; ++row) {
			const auto srcIdx{ pData.get() + row * lenX() };
			const auto dstIdx{ pCopy.get() + row * cols };
			std::move(EXEC_POLICY(unseq)
				srcIdx, srcIdx + minCols, dstIdx);
		}

		mRows = axis_size(rows);
		mCols = axis_size(cols);

		pData = std::move(pCopy);
		return *this;
	}
	#pragma endregion

	#pragma region resizeClean()
	constexpr self& resizeClean(size_type cols, size_type rows) {
		mCols = axis_size(cols);
		mRows = axis_size(rows);

		pData.reset();
		pData = ::allocate_n<T>(size()).as_value().release();
		return *this;
	}
	#pragma endregion

public:
	#pragma region initialize()
	/**
	 * @brief Initialize all of the matrix's data.
	 * @return Self reference for method chaining.
	 */
	constexpr self& initialize(T value = T{}) {
		std::fill(EXEC_POLICY(unseq)
			begin(), end(), value);
		return *this;
	}
	#pragma endregion

public:
	#pragma region initialize()
	/**
	 * @brief Initializes the matrix's data in a given direction.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to initialize. Directional.
	 * @param[in] cols :: Total columns to initialize. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 * @warning If the params exceed row/column length, all row data is initialized.
	 */
	constexpr self& initialize(difference_type cols, difference_type rows, T value = T{}) {
		if (const auto shift{ 0ull + std::abs(cols) }; shift) {
			if (cols < 0) {
				if (shift >= lenX()) { return initialize(value); }
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ end() - row * lenX() };
					std::fill(EXEC_POLICY(unseq)
						offset - shift, offset, value);
				}
			} else {
				if (shift >= lenX()) { return initialize(value); }
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ begin() + row * lenX() };
					std::fill(EXEC_POLICY(unseq)
						offset, offset + shift, value);
				}
			}
		}
		if (const auto shift{ 0ull + std::abs(rows) }; shift) {
			if (rows < 0) {
				if (shift >= lenY()) { return initialize(value); }
				std::fill(EXEC_POLICY(unseq)
					end() - shift * lenX(), end(), value);
			} else {
				if (shift >= lenY()) { return initialize(value); }
				std::fill(EXEC_POLICY(unseq)
					begin(), begin() + shift * lenX(), value);
			}
		}
		return *this;
	}
	#pragma endregion

	#pragma region rotate()
	/**
	 * @brief Rotates the matrix's data in a given direction.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to rotate. Directional.
	 * @param[in] cols :: Total columns to rotate. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 */
	constexpr self& rotate(difference_type cols, difference_type rows) {
		if (const auto shift{ 0ull + std::abs(cols) % lenX() }; shift) {
			if (cols < 0) {
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ begin() + row * lenX() };
					std::rotate(EXEC_POLICY(unseq)
						offset, offset + shift, offset + lenX());
				}
			} else {
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ begin() + row * lenX() };
					std::rotate(EXEC_POLICY(unseq)
						offset, offset + lenX() - shift, offset + lenX());
				}
			}
		}
		if (const auto shift{ 0ull + std::abs(rows) % lenY() * lenX() }; shift) {
			if (rows < 0) {
				std::rotate(EXEC_POLICY(unseq)
					begin(), begin() + shift, end());
			} else {
				std::rotate(EXEC_POLICY(unseq)
					begin(), end() - shift, end());
			}
		}
		return *this;
	}
	#pragma endregion

	#pragma region shift()
	/**
	 * @brief Shifts the matrix's data in a given direction. Combines the
	 *        functionality of rotating and wiping.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to shift. Directional.
	 * @param[in] cols :: Total columns to shift. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 * @warning If the params exceed row/column length, all row data is wiped.
	 */
	constexpr self& shift(difference_type cols, difference_type rows, T value = T{}) {
		return rotate(cols, rows).initialize(cols, rows, value);
	}
	#pragma endregion

	#pragma region reverse()
	/**
	 * @brief Reverses the matrix's data.
	 * @return Self reference for method chaining.
	 */
	constexpr self& reverse() {
		std::reverse(EXEC_POLICY(unseq)
			begin(), end());
		return *this;
	}
	#pragma endregion

	#pragma region flipY()
	/**
	 * @brief Reverses the matrix's data in row order.
	 * @return Self reference for method chaining.
	 */
	constexpr self& flipY() {
		const auto iterations{ lenY() >> 1 };
		for (size_type row{ 0u }; row < iterations; ++row) {
			const auto offset{ lenX() * row };
			std::swap_ranges(EXEC_POLICY(unseq)
				begin() + offset,
				begin() + offset + lenX(),
				end()   - offset - lenX()
			);
		}
		return *this;
	}
	#pragma endregion

	#pragma region flipX()
	/**
	 * @brief Reverses the matrix's data in column order.
	 * @return Self reference for method chaining.
	 */
	constexpr self& flipX() {
		for (size_type row{ 0u }; row < lenY(); ++row) {
			const auto offset{ begin() + lenX() * row };
			std::reverse(EXEC_POLICY(unseq)
				offset, offset + lenX());
		}
		return *this;
	}
	#pragma endregion

	#pragma region transpose()
	/**
	 * @brief Transposes the matrix's data. Works with rectangular dimensions.
	 * @return Self reference for method chaining.
	 */
	constexpr self& transpose() {
		if (size() > 1) {
			for (size_type a{ 1u }, b{ 1u }; a < size() - 1u; b = ++a) {
				do { b = (b % lenY()) * lenX() + (b / lenY()); }
					while (b < a);

				if (b != a) std::iter_swap(begin() + a, begin() + b);
			}
		}
		std::swap(mRows, mCols);
		return *this;
	}
	#pragma endregion

public:
	constexpr reference at(size_type idx) {
		if (idx >= size()) { throw std::out_of_range("Map2D.at() index out of range"); }
		return data()[idx];
	}
	constexpr reference at(size_type col, size_type row) {
		if (col >= lenX()) { throw std::out_of_range("Map2D.at() col out of range"); }
		if (row >= lenY()) { throw std::out_of_range("Map2D.at() row out of range"); }
		return data()[row * lenX() + col];
	}

	constexpr const_reference at(size_type idx) const {
		if (idx >= size()) { throw std::out_of_range("Map2D.at() index out of range"); }
		return data()[idx];
	}
	constexpr const_reference at(size_type col, size_type row) const {
		if (col >= lenX()) { throw std::out_of_range("Map2D.at() col out of range"); }
		if (row >= lenY()) { throw std::out_of_range("Map2D.at() row out of range"); }
		return data()[row * lenX() + col];
	}

	constexpr reference operator()(size_type idx) {
		assert(idx < size() && "Map2D.operator() index out of bounds");
		return data()[idx];
	}
	constexpr reference operator[](size_type idx) {
		assert(idx < size() && "Map2D.operator[] index out of bounds");
		return data()[idx];
	}

	constexpr const_reference operator()(size_type idx) const {
		assert(idx < size() && "Map2D.operator() index out of bounds");
		return data()[idx];
	}
	constexpr const_reference operator[](size_type idx) const {
		assert(idx < size() && "Map2D.operator[] index out of bounds");
		return data()[idx];
	}

	constexpr reference operator()(size_type col, size_type row) {
		assert(col < lenX() && "Map2D.operator() col out of bounds");
		assert(row < lenY() && "Map2D.operator() row out of bounds");
		return data()[row * lenX() + col];
	}
	#ifdef __cpp_multidimensional_subscript
	constexpr reference operator[](size_type col, size_type row) {
		assert(col < lenX() && "Map2D.operator[] col out of bounds");
		assert(row < lenY() && "Map2D.operator[] row out of bounds");
		return data()[row * lenX() + col];
	}
	#endif

	constexpr const_reference operator()(size_type col, size_type row) const {
		assert(col < lenX() && "Map2D.operator() col out of bounds");
		assert(row < lenY() && "Map2D.operator() row out of bounds");
		return data()[row * lenX() + col];
	}
	#ifdef __cpp_multidimensional_subscript
	constexpr const_reference operator[](size_type col, size_type row) const {
		assert(col < lenX() && "Map2D.operator[] col out of bounds");
		assert(row < lenY() && "Map2D.operator[] row out of bounds");
		return data()[row * lenX() + col];
	}
	#endif

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

template <typename T, std::size_t X, std::size_t Y, std::size_t N = sizeof(T)>
	requires (std::is_default_constructible_v<T>)
class alignas(N) FixedMap2D final {
	static_assert(X * Y >= 1,
		"FixedMap2D must have X and Y of at least 1.");
	static_assert((N & (N - 1)) == 0u,
		"N must be a power of two.");
	static_assert(N <= MAX_ALIGN,
		"Exceeded maximum allowed alignment.");

	using self = FixedMap2D;
	using self_flip = FixedMap2D<T, Y, X, N>;

public:
	using element_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type = std::remove_cv_t<T>;

	using pointer = T*;
	using const_pointer = const T*;

	using reference = T&;
	using const_reference = const T&;

	using iterator = T*;
	using const_iterator = const T*;

	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
	std::array<T, X * Y> pData{};

public:
	constexpr size_type size()       const noexcept { return X * Y; }
	constexpr size_type size_bytes() const noexcept { return size() * sizeof(value_type); }
	constexpr bool      empty()      const noexcept { return size() == 0; }
	constexpr auto      span()       const noexcept { return std::span(data(), size()); }

	constexpr size_type lenX() const noexcept { return X; }
	constexpr size_type lenY() const noexcept { return Y; }

	constexpr pointer   data()  { return pData.data(); }
	constexpr reference front() { return data()[0]; }
	constexpr reference back()  { return data()[size() - 1]; }

	constexpr const_pointer   data()  const { return pData.data(); }
	constexpr const_reference front() const { return data()[0]; }
	constexpr const_reference back()  const { return data()[size() - 1]; }

	constexpr auto first(size_type count) const { return RangeProxy(data(), count); }
	constexpr auto last (size_type count) const { return RangeProxy(data() + size() - count, count); }

	constexpr auto makeRowIter(size_type row)  const { return RangeIterator(data() + row * lenX(), lenX()); }
	constexpr auto makeRowProxy(size_type row) const { return *makeRowIter(row); }

	constexpr auto makeIter()  const { return RangeIterator(data(), size()); }
	constexpr auto makeProxy() const { return *makeIter(); }

	constexpr auto makeProxy2D() const { return RangeProxy2D(data(), lenX(), lenY()); }

	#pragma region Trivial Ctor
	constexpr FixedMap2D() noexcept = default;
	#pragma endregion

	#pragma region Copy/Move Ctor
	constexpr FixedMap2D(self&&) = default;
	constexpr FixedMap2D(self_flip&& other) {
		std::move(EXEC_POLICY(unseq)
			other.begin(), other.end(), begin());
	} // flipped
	constexpr FixedMap2D(const self& other) {
		std::copy(EXEC_POLICY(unseq)
			other.begin(), other.end(), begin());
	}
	constexpr FixedMap2D(const self_flip& other) { // flipped
		std::copy(EXEC_POLICY(unseq)
			other.begin(), other.end(), begin());
	}
	#pragma endregion

	#pragma region Move/Copy Assignment
	constexpr self& operator=(self&&) = default;
	constexpr self& operator=(self_flip&& other) {
		std::move(EXEC_POLICY(unseq)
			other.begin(), other.end(), begin());
		return *this;
	} // flipped
	constexpr self& operator=(const self& other) {
		if (this == &other) { return *this; }
		std::copy(EXEC_POLICY(unseq)
			other.begin(), other.end(), begin());
		return *this;
	}
	constexpr self& operator=(const self_flip& other) { // flipped
		std::copy(EXEC_POLICY(unseq)
			other.begin(), other.end(), begin());
		return *this;
	}
	#pragma endregion

public:
	#pragma region linearCopy() :: C++ Array
	/**
	 * @brief Copies data from another contiguous container. As much
	 *        as it can fit or pull.
	 * @returns Self reference for method chaining.
	 *
	 * @param[in] other :: Contiguous container to copy from.
	 */
	template <IsContiguousContainer Object>
	constexpr self& linearCopy(const Object& other) {
		std::copy_n(EXEC_POLICY(unseq)
			std::begin(other), std::min(size(), std::size(other)), begin());
		return *this;
	}
	#pragma endregion

	#pragma region linearCopy() :: C-style Array
	/**
	 * @brief Copies data from array pointer of the same type. As much
	 *        as it can fit or pull.
	 * @returns Self reference for method chaining.
	 *
	 * @param[in] other :: Pointer to array of data.
	 * @param[in] size  :: Amount of elements to copy (optional).
	 *
	 * @warning Cannot check if target array has sufficient data.
	 */
	template <size_type L>
	constexpr self& linearCopy(T(&other)[L], size_type len = L) {
		std::copy_n(EXEC_POLICY(unseq)
			other, std::min(len, size()), begin());
		return *this;
	}
	#pragma endregion

public:
	#pragma region initialize()
	/**
	 * @brief Initialize all of the matrix's data.
	 * @return Self reference for method chaining.
	 */
	constexpr self& initialize(T value = T{}) {
		std::fill(EXEC_POLICY(unseq)
			begin(), end(), value);
		return *this;
	}
	#pragma endregion

public:
	#pragma region initialize()
	/**
	 * @brief Initializes the matrix's data in a given direction.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to initialize. Directional.
	 * @param[in] cols :: Total columns to initialize. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 * @warning If the params exceed row/column length, all row data is initialized.
	 */
	constexpr self& initialize(difference_type cols, difference_type rows, T value = T{}) {
		if (const auto shift{ 0ull + std::abs(cols) }; shift) {
			if (cols < 0) {
				if (shift >= lenX()) { return initialize(value); }
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ end() - row * lenX() };
					std::fill(EXEC_POLICY(unseq)
						offset - shift, offset, value);
				}
			} else {
				if (shift >= lenX()) { return initialize(value); }
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ begin() + row * lenX() };
					std::fill(EXEC_POLICY(unseq)
						offset, offset + shift, value);
				}
			}
		}
		if (const auto shift{ 0ull + std::abs(rows) }; shift) {
			if (rows < 0) {
				if (shift >= lenY()) { return initialize(value); }
				std::fill(EXEC_POLICY(unseq)
					end() - shift * lenX(), end(), value);
			} else {
				if (shift >= lenY()) { return initialize(value); }
				std::fill(EXEC_POLICY(unseq)
					begin(), begin() + shift * lenX(), value);
			}
		}
		return *this;
	}
	#pragma endregion

	#pragma region rotate()
	/**
	 * @brief Rotates the matrix's data in a given direction.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to rotate. Directional.
	 * @param[in] cols :: Total columns to rotate. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 */
	constexpr self& rotate(difference_type cols, difference_type rows) {
		if (const auto shift{ 0ull + std::abs(cols) % lenX() }; shift) {
			if (cols < 0) {
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ begin() + row * lenX() };
					std::rotate(EXEC_POLICY(unseq)
						offset, offset + shift, offset + lenX());
				}
			} else {
				for (size_type row{ 0u }; row < lenY(); ++row) {
					const auto offset{ begin() + row * lenX() };
					std::rotate(EXEC_POLICY(unseq)
						offset, offset + lenX() - shift, offset + lenX());
				}
			}
		}
		if (const auto shift{ 0ull + std::abs(rows) % lenY() * lenX() }; shift) {
			if (rows < 0) {
				std::rotate(EXEC_POLICY(unseq)
					begin(), begin() + shift, end());
			} else {
				std::rotate(EXEC_POLICY(unseq)
					begin(), end() - shift, end());
			}
		}
		return *this;
	}
	#pragma endregion

	#pragma region shift()
	/**
	 * @brief Shifts the matrix's data in a given direction. Combines the
	 *        functionality of rotating and wiping.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to shift. Directional.
	 * @param[in] cols :: Total columns to shift. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 * @warning If the params exceed row/column length, all row data is wiped.
	 */
	constexpr self& shift(difference_type cols, difference_type rows, T value = T{}) {
		return rotate(cols, rows).initialize(cols, rows, value);
	}
	#pragma endregion

	#pragma region reverse()
	/**
	 * @brief Reverses the matrix's data.
	 * @return Self reference for method chaining.
	 */
	constexpr self& reverse() {
		std::reverse(EXEC_POLICY(unseq)
			begin(), end());
		return *this;
	}
	#pragma endregion

	#pragma region flipY()
	/**
	 * @brief Reverses the matrix's data in row order.
	 * @return Self reference for method chaining.
	 */
	constexpr self& flipY() {
		const auto iterations{ lenY() >> 1 };
		for (size_type row{ 0u }; row < iterations; ++row) {
			const auto offset{ lenX() * row };
			std::swap_ranges(EXEC_POLICY(unseq)
				begin() + offset,
				begin() + offset + lenX(),
				end() - offset - lenX()
			);
		}
		return *this;
	}
	#pragma endregion

	#pragma region flipX()
	/**
	 * @brief Reverses the matrix's data in column order.
	 * @return Self reference for method chaining.
	 */
	constexpr self& flipX() {
		for (size_type row{ 0u }; row < lenY(); ++row) {
			const auto offset{ begin() + lenX() * row };
			std::reverse(EXEC_POLICY(unseq)
				offset, offset + lenX());
		}
		return *this;
	}
	#pragma endregion

	#pragma region transpose()
	/**
	 * @brief Transposes the matrix's data. Works with rectangular dimensions.
	 * @return A copy of the transposed data.
	 */
	constexpr self_flip transpose() {
		self_flip other;

		std::for_each(EXEC_POLICY(unseq)
			other.begin(), other.end(),
			[this, begin = other.data()](T& value) noexcept {
				const auto i{ &value - begin };
				value = (*this)(i % Y, i / Y);
			});

		return other;
	}
	#pragma endregion

public:
	constexpr reference at(size_type idx) {
		if (idx >= size()) { throw std::out_of_range("FixedMap2D.at() index out of range"); }
		return data()[idx];
	}
	constexpr reference at(size_type col, size_type row) {
		if (col >= lenX()) { throw std::out_of_range("FixedMap2D.at() col out of range"); }
		if (row >= lenY()) { throw std::out_of_range("FixedMap2D.at() row out of range"); }
		return data()[row * lenX() + col];
	}
	constexpr const_reference at(size_type idx) const {
		if (idx >= size()) { throw std::out_of_range("FixedMap2D.at() index out of range"); }
		return data()[idx];
	}
	constexpr const_reference at(size_type col, size_type row) const {
		if (col >= lenX()) { throw std::out_of_range("FixedMap2D.at() col out of range"); }
		if (row >= lenY()) { throw std::out_of_range("FixedMap2D.at() row out of range"); }
		return data()[row * lenX() + col];
	}

	constexpr reference operator()(size_type idx) {
		assert(idx < size() && "FixedMap2D.operator() index out of bounds");
		return data()[idx];
	}
	constexpr reference operator[](size_type idx) {
		assert(idx < size() && "FixedMap2D.operator[] index out of bounds");
		return data()[idx];
	}
	constexpr const_reference operator()(size_type idx) const {
		assert(idx < size() && "FixedMap2D.operator() index out of bounds");
		return data()[idx];
	}
	constexpr const_reference operator[](size_type idx) const {
		assert(idx < size() && "FixedMap2D.operator[] index out of bounds");
		return data()[idx];
	}

	constexpr reference operator()(size_type col, size_type row) {
		assert(col < lenX() && "FixedMap2D.operator[] col out of bounds");
		assert(row < lenY() && "FixedMap2D.operator[] row out of bounds");
		return data()[row * lenX() + col];
	}
	#ifdef __cpp_multidimensional_subscript
	constexpr reference operator[](size_type col, size_type row) {
		assert(col < lenX() && "FixedMap2D.operator[] col out of bounds");
		assert(row < lenY() && "FixedMap2D.operator[] row out of bounds");
		return data()[row * lenX() + col];
	}
	#endif
	constexpr const_reference operator()(size_type col, size_type row) const {
		assert(col < lenX() && "FixedMap2D.operator[] col out of bounds");
		assert(row < lenY() && "FixedMap2D.operator[] row out of bounds");
		return data()[row * lenX() + col];
	}
	#ifdef __cpp_multidimensional_subscript
	constexpr const_reference operator[](size_type col, size_type row) const {
		assert(col < lenX() && "FixedMap2D.operator[] col out of bounds");
		assert(row < lenY() && "FixedMap2D.operator[] row out of bounds");
		return data()[row * lenX() + col];
	}
	#endif

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

#ifdef MAX_ALIGN
	#undef MAX_ALIGN
#endif
