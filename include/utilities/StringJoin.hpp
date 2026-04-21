/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <string_view>
#include <type_traits>

/*==================================================================*/

template <typename T>
constexpr std::size_t view_size(const T& t) noexcept {
	using U = std::remove_cvref_t<T>;
	/****/ if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
		return t.size();
	} else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
		return std::extent_v<U> -1; // C-style string literal: exclude null terminator
	} else if constexpr (std::is_pointer_v<U> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
		return std::char_traits<char>::length(t); // const char* null-terminated
	} else {
		return std::string_view(t).size(); // fallback for anything else convertible to string_view
	}
}

template <typename T>
constexpr const char* view_data(const T& t) noexcept {
	using U = std::remove_cvref_t<T>;
	/****/ if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
		return t.data();
	} else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
		return t;
	} else if constexpr (std::is_pointer_v<U> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
		return t;
	} else {
		return std::string_view(t).data(); // fallback
	}
}

/*==================================================================*/

template <typename... T> requires (
	(std::convertible_to<T, std::string_view> && ...)
	&& (sizeof...(T) > 1)
)
constexpr auto join(const T&... parts) noexcept {
	std::string result;

	result.reserve((view_size(parts) + ...));
	(result.append(view_data(parts), view_size(parts)), ...);

	return result;
}

template <typename... T> requires (
	(std::convertible_to<T, std::string_view> && ...)
	&& (sizeof...(T) > 1)
)
constexpr auto join_with(std::string_view sep, const T&... parts) noexcept {
	std::string result;

	result.reserve((view_size(parts) + ...)
		+ sizeof...(T) * sep.size());
	((result.append(view_data(parts), view_size(parts)).append(sep)), ...);

	result.resize(result.size() - sep.size());
	return result;
}

template <typename... T> requires (
	(std::convertible_to<T, std::string_view> && ...)
	&& (sizeof...(T) >= 1)
)
constexpr void join_into(std::string& src, const T&... parts) noexcept {
	src.reserve((view_size(parts) + ...) + src.size());
	(src.append(view_data(parts), view_size(parts)), ...);
}
