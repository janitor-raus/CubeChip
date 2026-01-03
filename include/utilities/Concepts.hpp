/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <iterator>
#include <concepts>
#include <type_traits>

/*==================================================================*/

template <typename T>
using ValueType = typename T::value_type;

template <typename T, typename U>
concept SameValueSizes = (sizeof(ValueType<T>) == sizeof(ValueType<U>));

template <typename T, typename U>
concept SameValueTypes = std::same_as<ValueType<T>, ValueType<U>>;

template <typename T, typename U>
concept MatchingValueType = std::same_as<std::remove_cv_t<T>, std::remove_cv_t<ValueType<U>>>;

/*==================================================================*/

template <typename T>
concept IsNothrowCopyable = std::is_nothrow_copy_constructible_v<T>
	&& std::is_nothrow_copy_assignable_v<T>;

template <typename T>
concept IsNothrowMovable = std::is_nothrow_move_constructible_v<T>
	&& std::is_nothrow_move_assignable_v<T>;

template <typename T, typename V>
concept IsNothrowConvertible = std::is_nothrow_convertible_v<T, V>;

template<typename T>
concept IsPlainOldData = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

/*==================================================================*/

template <typename T>
concept IsContiguousContainer = requires(const T& c) {
	typename T::value_type;
	requires std::same_as<
		std::remove_cv_t<ValueType<T>>,
		std::remove_cv_t<std::remove_pointer_t<decltype(std::data(c))>>
	>;
	{ std::size(c) } -> std::convertible_to<std::size_t>;
};

template <typename T, typename U>
concept IsContiguousContainerOf = (IsContiguousContainer<T> && std::same_as \
	<std::remove_cv_t<ValueType<T>>, std::remove_cv_t<U>>);
