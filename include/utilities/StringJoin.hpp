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
	} else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<U>>, char>) {
		return std::extent_v<U> - 1; // C-style string literal: exclude null terminator
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
	} else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<U>>, char>) {
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

// Less efficient than the STL version, but covers type gaps the STL doesn't.
template <typename T1, typename T2> requires (
	std::convertible_to<T1, std::string_view> &&
	std::convertible_to<T2, std::string_view>
)
constexpr auto operator+(const T1& lhs, const T2& rhs) noexcept {
	return join(lhs, rhs);
}

/*==================================================================*/

template <typename... T> requires (
	(std::convertible_to<T, std::string_view> && ...)
	&& (sizeof...(T) >= 1)
)
constexpr void join_into(std::string& src, const T&... parts) noexcept {
	src.reserve((view_size(parts) + ...) + src.size());
	(src.append(view_data(parts), view_size(parts)), ...);
}

/*==================================================================*/

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
	&& (sizeof...(T) > 1)
)
constexpr auto join_with(char sep, const T&... parts) noexcept {
	return join_with(std::string_view(&sep, 1), parts...);
}

/*==================================================================*/

#ifdef _WIN32
inline constexpr const char* path_separator = "\\";
#else
inline constexpr const char* path_separator = "/";
#endif

constexpr bool is_separator(char c) noexcept {
#ifdef _WIN32
	return c == '\\' || c == '/'; // Windows accepts both
#else
	return c == '/';              // POSIX only recognizes '/'
#endif
}

// Lightweight path joining operator, behaves similarly to the STL's
// filesystem 'operator/()' but without the overhead of the whole header.
// If 'lhs' is empty, returns 'rhs'.
// If 'rhs.front() == separator', returns 'rhs'.
// If 'lhs.back() == separator', returns 'lhs + rhs'.
// If none of the above, returns 'lhs + separator + rhs'.
template <typename T1, typename T2> requires (
	std::convertible_to<T1, std::string_view> &&
	std::convertible_to<T2, std::string_view>
)
constexpr auto operator/(const T1& lhs, const T2& rhs) noexcept {
	std::string_view a = lhs;
	std::string_view b = rhs;

	if (a.size() == 0) { return std::string(rhs); }
	if (b.size() > 0 && is_separator(b.front()))
		[[unlikely]] { return std::string(rhs); }

	return is_separator(a.back()) ? lhs + rhs
		: join(lhs, path_separator, rhs);
}

// Does not validate path correctness, just joins the parts with the OS-specific separator.
template <typename... T> requires (
	(std::convertible_to<T, std::string_view> && ...)
	&& (sizeof...(T) > 1)
)
constexpr auto join_path(const T&... parts) noexcept {
	return join_with(path_separator, parts...);
}
