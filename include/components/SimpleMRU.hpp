/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <type_traits>
#include <cstddef>
#include <algorithm>
#include <array>
#include <span>

/*==================================================================*/

template <typename T, std::size_t Capacity> requires(
	std::is_nothrow_move_assignable_v<T> &&
	std::is_nothrow_move_constructible_v<T>
)
class SimpleMRU {
	static_assert(Capacity > 1 && Capacity <= 50,
		"SimpleMRU has a capacity limit of 1 < X <= 50.");

	std::array<T, Capacity> m_items{};
	std::size_t m_size{};

public:
	constexpr auto size()     const noexcept { return m_size; }
	constexpr auto capacity() const noexcept { return Capacity; }

	const T& operator[](std::size_t i) const noexcept { return m_items[i]; }
	      T& operator[](std::size_t i)       noexcept { return m_items[i]; }

	auto span() const noexcept { return std::span<const T>(m_items.data(), m_size); }
	auto span()       noexcept { return std::span<      T>(m_items.data(), m_size); }

	void clear() noexcept { m_size = 0; }

	template <typename U> requires(std::convertible_to<U, T>)
	void insert(U&& entry) noexcept { insert_impl(std::forward<U>(entry)); }

private:
	auto find(const T* entry) const noexcept {
		return std::distance(m_items.begin(), std::find(
			m_items.begin(), m_items.end(), *entry));
	}

	void insert_in_front(T* entry) noexcept {
		if (m_size < Capacity) { ++m_size; }

		m_items[m_size - 1] = std::move(*entry);
		std::rotate(
			m_items.begin(),
			m_items.begin() + (m_size - 1),
			m_items.begin() + m_size
		);
	}

	void promote_existing(std::size_t pos) noexcept {
		std::rotate(
			m_items.begin(),
			m_items.begin() + pos,
			m_items.begin() + pos + 1
		);
	}

	void insert_impl(T&& value) noexcept {
		auto pos = find(&value);

		switch (pos) {
			case 0: return;
			case Capacity: insert_in_front(&value); break;
			default:       promote_existing(pos);   break;
		}
	}
};
