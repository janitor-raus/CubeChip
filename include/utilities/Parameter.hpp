/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <algorithm>
#include <concepts>

/*==================================================================*/

template <auto Def, auto Min, auto Max> requires (
	std::convertible_to<decltype(Min), decltype(Def)> &&
	std::convertible_to<decltype(Max), decltype(Def)>
) class BoundedParam {
	static_assert(static_cast<decltype(Def)>(Min) < static_cast<decltype(Def)>(Max),
		"Min value is not lesser than Max value");
	static_assert(Def >= static_cast<decltype(Def)>(Min) && Def <= static_cast<decltype(Def)>(Max),
		"Def value is not within [Min..Max] bounds");

	using T = decltype(Def);
	T value;

	static constexpr T clamp(T x) noexcept {
		return std::clamp(x, T(Min), T(Max));
	}

public:
	static constexpr T def = T(Def);
	static constexpr T min = T(Min);
	static constexpr T max = T(Max);

	BoundedParam() noexcept : value(Def) {}

	template <typename U> requires (std::convertible_to<U, T>)
	BoundedParam(U x) noexcept : value(clamp(T(x))) {}

	constexpr T get() const noexcept { return value; }

	constexpr T& operator*()  noexcept { return  value; }
	constexpr T* operator->() noexcept { return &value; }

	constexpr const T& operator*()  const noexcept { return  value; }
	constexpr const T* operator->() const noexcept { return &value; }

	constexpr operator T() const noexcept { return value; }

	/*==================================================================*/

	template <typename U> requires (std::convertible_to<U, T>)
	constexpr BoundedParam& operator+=(U x) noexcept {
		value = clamp(static_cast<T>(value + x)); return *this;
	}

	template <typename U> requires (std::convertible_to<U, T>)
	constexpr BoundedParam& operator-=(U x) noexcept {
		value = clamp(static_cast<T>(value - x)); return *this;
	}

	template <typename U> requires (std::convertible_to<U, T>)
	constexpr BoundedParam& operator*=(U x) noexcept {
		value = clamp(static_cast<T>(value * x)); return *this;
	}

	template <typename U> requires (std::convertible_to<U, T>)
	constexpr BoundedParam& operator/=(U x) {
		value = clamp(static_cast<T>(value / x)); return *this;
	}
};
