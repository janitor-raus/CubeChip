/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <numeric>
#include <cstddef>
#include <algorithm>

#include "Concepts.hpp"
#include "ExecPolicy.hpp"

/*==================================================================*/

template <IsContiguousContainer Object>
	requires (!std::is_rvalue_reference_v<Object>)
inline signed accumulate(Object& array, signed val = {}) noexcept {
	return std::accumulate(
		std::begin(array), std::end(array), val);
}

template <IsContiguousContainer Object, typename V = ValueType<Object>>
	requires (!std::is_rvalue_reference_v<Object> && std::convertible_to<V, ValueType<Object>>)
inline void fill(Object& array, V val = {}) noexcept {
	std::fill(EXEC_POLICY(unseq)
		std::begin(array), std::end(array),
		static_cast<ValueType<Object>>(val));
}

template <IsContiguousContainer Object, typename V = ValueType<Object>>
	requires (!std::is_rvalue_reference_v<Object> && std::convertible_to<V, ValueType<Object>>)
inline void fill_n(Object& array, std::size_t offset = 0, std::size_t count = 0, V val = {}) noexcept {
	if (offset < std::size(array)) {
		std::fill_n(EXEC_POLICY(unseq)
			std::data(array) + offset, count ? count : std::size(array) - offset,
			static_cast<ValueType<Object>>(val));
	}
}

template <IsContiguousContainer Object, std::invocable Generator>
	requires (!std::is_rvalue_reference_v<Object>)
inline void generate(Object& array, Generator&& lambda) noexcept {
	std::generate(EXEC_POLICY(unseq)
		std::begin(array), std::end(array),
		std::forward<Generator>(lambda));
}

template <IsContiguousContainer Object, std::invocable Generator>
	requires (!std::is_rvalue_reference_v<Object>)
inline void generate_n(Object& array, std::size_t offset, std::size_t count, Generator&& lambda) noexcept {
	if (offset < std::size(array)) {
		std::generate_n(EXEC_POLICY(unseq)
			std::data(array) + offset,
			count ? count : std::size(array) - offset,
			std::forward<Generator>(lambda));
	}
}

/*==================================================================*/

/**
 * @brief CRTP facade for simple container-like classes. Offers basic
 * iterator support and common container methods. Requires the Derived
 * to implement `data()` and `size()` methods. Does not offer direct
 * element access methods.
 * @tparam Derived The derived class inheriting the facade.
 * @tparam T The type of elements stored in the container.
 */
template <typename Derived, typename T>
	requires (std::is_class_v<Derived> && std::is_object_v<T>)
struct SimpleContainerFacade {
	using value_type      = T;
	using size_type       = std::size_t;
	using difference_type = std::ptrdiff_t;
	using pointer         = value_type*;
	using const_pointer   = const value_type*;
	using reference       = value_type&;
	using const_reference = const value_type&;

	using iterator       = pointer;
	using const_iterator = const_pointer;

	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr iterator begin() noexcept { return self().data(); }
	constexpr iterator end()   noexcept { return self().data() + self().size(); }
	constexpr const_iterator begin() const noexcept { return self().data(); }
	constexpr const_iterator end()   const noexcept { return self().data() + self().size(); }
	constexpr const_iterator cbegin() const noexcept { return begin(); }
	constexpr const_iterator cend()   const noexcept { return end(); }

	constexpr reverse_iterator rbegin() noexcept { return std::reverse_iterator(end()); }
	constexpr reverse_iterator rend()   noexcept { return std::reverse_iterator(begin()); }
	constexpr const_reverse_iterator rbegin() const noexcept { return std::reverse_iterator(end()); }
	constexpr const_reverse_iterator rend()   const noexcept { return std::reverse_iterator(begin()); }
	constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
	constexpr const_reverse_iterator crend()   const noexcept { return rend(); }

	constexpr auto empty() const noexcept { return self().size() == 0; }

	constexpr reference front() noexcept { return *begin(); }
	constexpr reference back()  noexcept { return *rbegin(); }
	constexpr const_reference front() const noexcept { return *cbegin(); }
	constexpr const_reference back()  const noexcept { return *crbegin(); }

private:
	constexpr       Derived& self()       noexcept { return *static_cast<      Derived*>(this); }
	constexpr const Derived& self() const noexcept { return *static_cast<const Derived*>(this); }
};
