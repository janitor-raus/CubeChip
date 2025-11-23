/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string_view>

#include "Expected.hpp"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

/*==================================================================*/

namespace config {
	// copies key values from the right table to the left, if they exist already
	void safeTableUpdate(toml::table& dst, const toml::table& src);
	void safeTableInsert(toml::table& dst, const toml::table& src);

	auto writeToFile(const toml::table& table, const char* filename) noexcept
		-> Expected<bool, std::error_code>;

	auto parseFromFile(const char* filename) noexcept
		-> toml::parse_result;

	template <typename T>
	inline void get(const toml::table& src, std::string_view key, T& dst) {
		if (auto val = src.at_path(key).value<T>())
			{ dst = static_cast<T>(*val); }
	}

	template <typename T>
	inline void set(toml::table& dst, std::string_view key, T src = T{}) {
		auto* current = &dst;
		auto start = key.begin();

		while (start != key.end()) {
			auto end = std::find(start, key.end(), '.');
			std::string_view subkey(start, end);

			if (end == key.end()) {
				current->insert_or_assign(subkey, src);
				return;
			}

			if (!current->contains(subkey)) {
				current->insert(subkey, toml::table{});
			}

			current = current->get(subkey)->as_table();
			if (!current) { return; }
			else { start = end + 1; }
		}
	}
}
