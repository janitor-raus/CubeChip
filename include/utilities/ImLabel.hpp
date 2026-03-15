/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "StringJoin.hpp"

/*==================================================================*/

/**
 * @brief Utility class for handling ImGui labels.
 *
 * ImGui uses '###' as a separator between the visible name and an internal
 * hashable tag. This class encapsulates that functionality, allowing easy
 * manipulation of both the name and tag components.
 */
struct ImLabel {
	static constexpr const char* separator = "###";

	std::string value;

	constexpr ImLabel() noexcept = default;
	constexpr ImLabel(std::string&& v)    noexcept : value(std::move(v)) {}
	constexpr ImLabel(std::string_view v) noexcept : value(v) {}
	constexpr ImLabel(const char* v)      noexcept : value(v ? v : "") {}

	template <typename T1, typename T2> requires (
		(std::convertible_to<T1, std::string_view>) &&
		(std::convertible_to<T2, std::string_view>)
	)
	constexpr ImLabel(const T1& name, const T2& id) noexcept
		: value(::join(name, separator, id))
	{}

	/*==================================================================*/

	constexpr operator std::string_view() const noexcept { return value; }
	constexpr operator const char*()      const noexcept { return value.c_str(); }
	constexpr operator bool()             const noexcept { return !value.empty(); }

	constexpr       std::string* operator->()       noexcept { return &value; }
	constexpr const std::string* operator->() const noexcept { return &value; }

	constexpr       std::string& operator*()       noexcept { return value; }
	constexpr const std::string& operator*() const noexcept { return value; }

	constexpr bool operator==(const ImLabel& other) const noexcept {
		return value == other.value;
	}

	/*==================================================================*/
private:
	constexpr auto find_separator() const noexcept {
		return value.find(separator);
	}

public:
	constexpr bool has_id() const noexcept {
		return find_separator() != std::string::npos;
	}

	constexpr bool has_name() const noexcept {
		return find_separator() - 1 < std::string::npos - 1;
	}

	constexpr std::string_view get_name() const noexcept {
		const auto pos = find_separator();
		return (pos == std::string::npos) ? value
			: std::string_view(value.data(), pos);
	}

	constexpr std::string_view get_id() const noexcept {
		const auto pos = find_separator();
		return (pos == std::string::npos) ? std::string_view()
			: std::string_view(value.data() + pos);
	}

	constexpr std::string_view get_id_or_label() const noexcept {
		const auto pos = find_separator();
		return (pos == std::string::npos) ? value
			: std::string_view(value.data() + pos);
	}

	/*==================================================================*/

	constexpr ImLabel& set_name(std::string_view new_name) noexcept {
		value = ::join(new_name, get_id());
		return *this;
	}

	constexpr ImLabel& set_id(std::string_view new_id) noexcept {
		value = ::join(get_name(), separator, new_id);
		return *this;
	}

	constexpr ImLabel& set_label(std::string_view new_name, std::string_view new_id) noexcept {
		value = ::join(new_name, separator, new_id);
		return *this;
	}
};

/*==================================================================*/

namespace std {
	template<>
	struct hash<ImLabel> {
		std::size_t operator()(const ImLabel& label) const noexcept {
			return std::hash<std::string>{}(label.value);
		}
	};
}
