/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <type_traits>
#include <cstddef>
#include <algorithm>
#include <vector>
#include <span>

/*==================================================================*/

template <typename T> requires(
	std::is_nothrow_move_assignable_v<T> &&
	std::is_nothrow_move_constructible_v<T>
)
class SimpleMRU {
	std::vector<T> m_items;

public:
	using value_type = T;

	auto size() const noexcept { return m_items.size(); }

	bool empty() const noexcept { return m_items.empty(); }
	void clear() noexcept { m_items.clear(); }

	const T& operator[](std::size_t i) const noexcept { return m_items[i]; }
	      T& operator[](std::size_t i)       noexcept { return m_items[i]; }

	auto span() const noexcept { return std::span<const T>(m_items); }
	auto span()       noexcept { return std::span<      T>(m_items); }

	template <typename U> requires std::convertible_to<U, T>
	void insert(U&& value) noexcept {
		auto it = std::find(m_items.begin(), m_items.end(), value);
		if (it != m_items.end()) {
			std::rotate(m_items.begin(), it, it + 1);
		} else {
			m_items.insert(m_items.begin(), value);
		}
	}

	template <typename U> requires(std::convertible_to<U, T>)
	bool erase(const U& value) noexcept {
		auto it = std::find(m_items.begin(), m_items.end(), value);
		if (it == m_items.end()) { return false; }
		else { m_items.erase(it); return true; }
	}
};
