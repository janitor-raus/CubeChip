/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <span>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <utility>
#include <stdexcept>

#include "ExecPolicy.hpp"
#include "Concepts.hpp"

/*==================================================================*/

/**
 * @brief Block-based iterator for contiguous containers, where each increment moves
 *        the iterator by the size of the entire span.
 * @tparam T The type of elements in the span. The iterator will yield spans of this type.
 */
template <typename T>
struct SpanIterator final {
	using element_type    = T;
	using size_type       = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type      = std::remove_cv_t<std::span<T>>;

	using iterator_category = std::contiguous_iterator_tag;

	using pointer        = std::span<T>*;
	using const_pointer  = const std::span<T>*;

	using reference       = std::span<T>&;
	using const_reference = const std::span<T>&;

protected:
	std::span<T> m_span;

public:
	constexpr SpanIterator(T* begin, size_type length) noexcept
		: m_span(begin, length)
	{}

	template <std::size_t N>
	constexpr SpanIterator(T(&array)[N]) noexcept
		: m_span(array, N)
	{}

	template <IsContiguousContainer Object>
		requires (!std::is_rvalue_reference_v<Object>)
	constexpr SpanIterator(const Object& array) noexcept
		: m_span(std::data(array), std::size(array))
	{}

public:
	constexpr auto& operator* () const noexcept { return  m_span; }
	constexpr auto* operator->() const noexcept { return &m_span; }

	constexpr auto& operator++() noexcept { m_span = std::span(m_span.data() + m_span.size(), m_span.size()); return *this; }
	constexpr auto& operator--() noexcept { m_span = std::span(m_span.data() - m_span.size(), m_span.size()); return *this; }

	constexpr auto& operator+=(difference_type rhs) noexcept { m_span = std::span(m_span.data() + rhs * m_span.size(), m_span.size()); return *this; }
	constexpr auto& operator-=(difference_type rhs) noexcept { m_span = std::span(m_span.data() - rhs * m_span.size(), m_span.size()); return *this; }

	constexpr auto  operator++(int) noexcept { auto tmp = *this; m_span = std::span(m_span.data() + m_span.size(), m_span.size()); return tmp; }
	constexpr auto  operator--(int) noexcept { auto tmp = *this; m_span = std::span(m_span.data() - m_span.size(), m_span.size()); return tmp; }

	constexpr auto  operator+ (difference_type rhs) const noexcept { return SpanIterator(m_span.data() + rhs * m_span.size(), m_span.size()); }
	constexpr auto  operator- (difference_type rhs) const noexcept { return SpanIterator(m_span.data() - rhs * m_span.size(), m_span.size()); }

	constexpr friend auto operator+(difference_type lhs, const SpanIterator& rhs) noexcept { return rhs + lhs; }
	constexpr friend auto operator-(difference_type lhs, const SpanIterator& rhs) noexcept { return rhs - lhs; }

	constexpr difference_type operator-(const SpanIterator& other) const noexcept { return m_span.data() - other.m_span.data(); }

	constexpr bool operator==(const SpanIterator& other) const noexcept { return m_span.data() == other.m_span.data(); }
	constexpr bool operator!=(const SpanIterator& other) const noexcept { return m_span.data() != other.m_span.data(); }
	constexpr bool operator< (const SpanIterator& other) const noexcept { return m_span.data() <  other.m_span.data(); }
	constexpr bool operator> (const SpanIterator& other) const noexcept { return m_span.data() >  other.m_span.data(); }
	constexpr bool operator<=(const SpanIterator& other) const noexcept { return m_span.data() <= other.m_span.data(); }
	constexpr bool operator>=(const SpanIterator& other) const noexcept { return m_span.data() >= other.m_span.data(); }

	constexpr auto operator[](difference_type rhs) const noexcept { return std::span<T>(m_span.data() + rhs * m_span.size(), m_span.size()); }
};

template <typename Container>
SpanIterator(Container&) -> SpanIterator<std::remove_pointer_t<decltype(
		std::data(std::declval<Container&>()))>>;

/*==================================================================*/

template <typename T>
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
	pointer   m_data;
	axis_size m_size_x;
	axis_size m_size_y;

public:
	constexpr size_type size()       const noexcept { return m_size_y * m_size_x; }
	constexpr size_type size_bytes() const noexcept { return size() * sizeof(value_type); }
	constexpr bool      empty()      const noexcept { return size() == 0; }
	constexpr auto      span()       const noexcept { return std::span(data(), size()); }

	constexpr axis_size width()  const noexcept { return m_size_x; }
	constexpr axis_size height() const noexcept { return m_size_y; }

	constexpr pointer       data()       noexcept { return m_data; }
	constexpr const_pointer data() const noexcept { return m_data; }

	constexpr reference       front()       { return data()[0]; }
	constexpr const_reference front() const { return data()[0]; }

	constexpr reference       back()       { return data()[size() - 1]; }
	constexpr const_reference back() const { return data()[size() - 1]; }

	constexpr auto first(size_type count) const { return *SpanIterator(data(), count); }
	constexpr auto last (size_type count) const { return *SpanIterator(data() + size() - count, count); }

	constexpr auto make_row_iter(size_type row)  const { return SpanIterator(data() + row * width(), width()); }
	constexpr auto make_row_proxy(size_type row) const { return *make_row_iter(row); }

	constexpr auto make_map_iter()  const { return SpanIterator(data(), size()); }
	constexpr auto make_map_proxy() const { return *make_map_iter(); }

/*==================================================================*/

	constexpr Map2D() noexcept = default;
	constexpr Map2D(pointer ptr, size_type cols = 1, size_type rows = 1) noexcept
		: m_data(ptr)
		, m_size_x(axis_size(cols))
		, m_size_y(axis_size(rows))
	{}
	constexpr Map2D(std::span<T> span, size_type cols = 1, size_type rows = 1) noexcept
		: Map2D(span.data(), cols, rows)
	{}

	// manually change the map's data pointer (use with caution)
	constexpr self& reseat(pointer data) noexcept {
		m_data = data;
		return *this;
	}

	// manually change the map's data pointer (use with caution)
	constexpr self& reseat(std::span<T> span) noexcept {
		return reseat(span.data());
	}

	// manually change the map's size (use with caution)
	constexpr self& resize(size_type width, size_type height) noexcept {
		m_size_x = axis_size(width);
		m_size_y = axis_size(height);
		return *this;
	}

/*==================================================================*/

public:
	/**
	 * @brief Fill all of the matrix's data.
	 * @return Self reference for method chaining.
	 */
	self& fill(T value = T{}) {
		std::fill(EXEC_POLICY(unseq)
			begin(), end(), value);
		return *this;
	}

public:
	/**
	 * @brief Fills the matrix's data in a given direction.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to fill. Directional.
	 * @param[in] cols :: Total columns to fill. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 * @warning If the params exceed row/column length, all row data is fill.
	 */
	self& fill(difference_type cols, difference_type rows, T value = T{}) {
		if (const auto shift = size_type(std::abs(cols)); shift != 0) {
			if (cols < 0) {
				if (shift >= width()) { return fill(value); }
				for (size_type row = 0; row < height(); ++row) {
					const auto offset = end() - row * width();
					std::fill(EXEC_POLICY(unseq)
						offset - shift, offset, value);
				}
			} else {
				if (shift >= width()) { return fill(value); }
				for (size_type row = 0; row < height(); ++row) {
					const auto offset = begin() + row * width();
					std::fill(EXEC_POLICY(unseq)
						offset, offset + shift, value);
				}
			}
		}
		if (const auto shift = size_type(std::abs(rows)); shift != 0) {
			if (rows < 0) {
				if (shift >= height()) { return fill(value); }
				std::fill(EXEC_POLICY(unseq)
					end() - shift * width(), end(), value);
			} else {
				if (shift >= height()) { return fill(value); }
				std::fill(EXEC_POLICY(unseq)
					begin(), begin() + shift * width(), value);
			}
		}
		return *this;
	}

	/**
	 * @brief Rotates the matrix's data in a given direction.
	 * @return Self reference for method chaining.
	 *
	 * @param[in] rows :: Total rows to rotate. Directional.
	 * @param[in] cols :: Total columns to rotate. Directional.
	 *
	 * @warning The sign of the params control the application direction.
	 */
	self& rotate(difference_type cols, difference_type rows) {
		if (const auto shift = size_type(std::abs(cols)) % width(); shift != 0) {
			if (cols < 0) {
				for (size_type row = 0; row < height(); ++row) {
					const auto offset = begin() + row * width();
					std::rotate(EXEC_POLICY(unseq)
						offset, offset + shift, offset + width());
				}
			} else {
				for (size_type row = 0; row < height(); ++row) {
					const auto offset = begin() + row * width();
					std::rotate(EXEC_POLICY(unseq)
						offset, offset + width() - shift, offset + width());
				}
			}
		}
		if (const auto shift = size_type(std::abs(rows)) % height() * width(); shift != 0) {
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
	self& shift(difference_type cols, difference_type rows, T value = T{}) {
		return rotate(cols, rows).fill(cols, rows, value);
	}

	/**
	 * @brief Reverses the matrix's data.
	 * @return Self reference for method chaining.
	 */
	self& reverse() {
		std::reverse(EXEC_POLICY(unseq)
			begin(), end());
		return *this;
	}

	/**
	 * @brief Reverses the matrix's data in row order.
	 * @return Self reference for method chaining.
	 */
	self& flip_y() {
		const auto iterations = height() / 2;
		for (size_type row = 0; row < iterations; ++row) {
			const auto offset = width() * row;
			std::swap_ranges(EXEC_POLICY(unseq)
				begin() + offset,
				begin() + offset + width(),
				end()   - offset - width()
			);
		}
		return *this;
	}

	/**
	 * @brief Reverses the matrix's data in column order.
	 * @return Self reference for method chaining.
	 */
	self& flip_x() {
		for (size_type row = 0; row < height(); ++row) {
			const auto offset{ begin() + width() * row };
			std::reverse(EXEC_POLICY(unseq)
				offset, offset + width());
		}
		return *this;
	}

	/**
	 * @brief Transposes the matrix's data. Works with rectangular dimensions.
	 * @return Self reference for method chaining.
	 */
	self& transpose() {
		if (size() > 1) {
			for (size_type a = 1, b = 1; a < size() - 1; b = ++a) {
				do { b = (b % height()) * width() + (b / height()); }
					while (b < a);

				if (b != a) std::iter_swap(begin() + a, begin() + b);
			}
		}
		std::swap(m_size_y, m_size_x);
		return *this;
	}

public:
	constexpr reference at(size_type idx) {
		if (idx >= size()) { throw std::out_of_range("at() index out of range"); }
		return data()[idx];
	}
	constexpr reference at(size_type col, size_type row) {
		if (col >= width()) { throw std::out_of_range("at() col out of range"); }
		if (row >= height()) { throw std::out_of_range("at() row out of range"); }
		return data()[row * width() + col];
	}

	constexpr const_reference at(size_type idx) const {
		if (idx >= size()) { throw std::out_of_range("at() index out of range"); }
		return data()[idx];
	}
	constexpr const_reference at(size_type col, size_type row) const {
		if (col >= width()) { throw std::out_of_range("at() col out of range"); }
		if (row >= height()) { throw std::out_of_range("at() row out of range"); }
		return data()[row * width() + col];
	}

	constexpr reference operator()(size_type idx) {
		assert(idx < size() && "operator() index out of bounds");
		return data()[idx];
	}
	constexpr reference operator[](size_type idx) {
		assert(idx < size() && "operator[] index out of bounds");
		return data()[idx];
	}

	constexpr const_reference operator()(size_type idx) const {
		assert(idx < size() && "operator() index out of bounds");
		return data()[idx];
	}
	constexpr const_reference operator[](size_type idx) const {
		assert(idx < size() && "operator[] index out of bounds");
		return data()[idx];
	}

	constexpr reference operator()(size_type col, size_type row) {
		assert(col < width() && "operator() col out of bounds");
		assert(row < height() && "operator() row out of bounds");
		return data()[row * width() + col];
	}
	#ifdef __cpp_multidimensional_subscript
	constexpr reference operator[](size_type col, size_type row) {
		assert(col < width() && "operator[] col out of bounds");
		assert(row < height() && "operator[] row out of bounds");
		return data()[row * width() + col];
	}
	#endif

	constexpr const_reference operator()(size_type col, size_type row) const {
		assert(col < width() && "operator() col out of bounds");
		assert(row < height() && "operator() row out of bounds");
		return data()[row * width() + col];
	}
	#ifdef __cpp_multidimensional_subscript
	constexpr const_reference operator[](size_type col, size_type row) const {
		assert(col < width() && "operator[] col out of bounds");
		assert(row < height() && "operator[] row out of bounds");
		return data()[row * width() + col];
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
