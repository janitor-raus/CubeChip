/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <version>
#include <utility>

/*==================================================================*/

#if defined(__has_include) && (__has_include(<expected>)) \
&& defined(__cpp_lib_expected) && (__cpp_lib_expected >= 202202L)
	#include <expected>

	template <typename T, typename E>
	using Expected = std::expected<T, E>;

	template <typename E>
	using Unexpected = std::unexpected<E>;
#else
	#include <tl/expected.hpp>

	template <typename T, typename E>
	using Expected = tl::expected<T, E>;

	template <typename E>
	using Unexpected = tl::unexpected<E>;
#endif

/*==================================================================*/

/**
 * @brief Creates an Unexpected<E> object from an error.
 *
 * @tparam E Type of the error.
 * @param error The error to be wrapped in Unexpected.
 * @return An Unexpected<E> object containing the error.
 * @warning Currently does not work with lvalue E types, std::move them first.
 */
template <typename E>
	requires (requires (E e) { !e; })
inline constexpr Unexpected<E> make_unexpected(E&& error) {
	return Unexpected<E>(std::forward<E>(error));
}

/**
 * @brief Creates an Expected<T, E> object from a value and an error.
 * If the error evaluates to false, it returns the value.
 * Otherwise, it returns an Unexpected<E> object with the error.
 *
 * @tparam T Type of the value.
 * @tparam E Type of the error.
 * @param value The value to be wrapped in Expected.
 * @param error The error to be checked and possibly wrapped in Unexpected.
 * @return An Expected<T, E> object containing either the value or the error.
 * @warning Currently does not work with lvalue E types, std::move them first.
 */
template <typename T, typename E>
	requires (requires (E e) { !e; })
inline constexpr Expected<T, E> make_expected(T&& value, E&& error) {
	if (!error) { return std::forward<T>(value); }
	else { return Unexpected<E>(std::forward<E>(error)); }
}
