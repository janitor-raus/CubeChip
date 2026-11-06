/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

/*==================================================================*/

/**
 * @brief Fixed-size, null-terminated string key with automatic truncation.
 *
 * Stores up to 15 characters plus a null terminator. Strings exceeding
 * the available capacity are truncated, with the final character replaced
 * by '~' to indicate truncation. Supports mutable and const C-string access,
 * string comparison, and hashing.
 */
struct ShortKey {
	static constexpr std::size_t size = 16;
	char data[size]{};

	constexpr ShortKey() noexcept = default;
	constexpr explicit ShortKey(const char* src) noexcept {
		if (!src || src[0] == '\0') { return; }
		for (std::size_t i = 0; i < (size - 1); ++i) {
			if ((data[i] = src[i]) == '\0') { return; }
		}

		// if we reached this point, the string was
		// too long to fit and has to be truncated!
		data[size - 2] = '~';
		data[size - 1] = '\0';
	}

	constexpr operator /***/ char* () /***/ noexcept { return data; }
	constexpr operator const char* () const noexcept { return data; }

	constexpr const char* c_str() const noexcept { return data; }

	bool operator==(const ShortKey& other) const noexcept {
		return std::strcmp(data, other.data) == 0;
	}

	struct Hash {
		std::size_t operator()(const ShortKey& key) const noexcept {
			return std::hash<std::string_view>{}(key.data);
		}
	};
};
