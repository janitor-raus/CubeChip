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

struct TomlConfig {
	// copies key values from the right table to the left, if they exist already
	static void safeTableUpdate(toml::table& dst, const toml::table& src);
	static void safeTableInsert(toml::table& dst, const toml::table& src);

	static auto writeToFile(const toml::table& table, const char* filename) noexcept
		-> Expected<bool, std::error_code>;

	static auto parseFromFile(const char* filename) noexcept
		-> toml::parse_result;

	template <typename T>
	static void get(const toml::table& src, std::string_view key, T* dst, std::size_t elem_count) noexcept {
		switch (elem_count) {
			case 0:
				break;

			case 1:
				*dst = src.at_path(key).value_or(T());
				break;

			default:
				for (std::size_t i = 0; i < elem_count; ++i) { dst[i] = T(); }

				if (auto* array = src.at_path(key).as_array()) {
					const auto limit = std::min(elem_count, array->size());

					for (std::size_t i = 0; i < limit; ++i) {
						dst[i] = (*array)[i].value_or(T());
					}
				}
		}
	}

	template <typename T>
	static void set(toml::table& dst, std::string_view key, const T* src, std::size_t elem_count = 1) noexcept {
		switch (elem_count) {
			case 0:
				break;

			case 1:
				insert_at(dst, key, src[0]);
				break;

			default: {
				toml::array array;
				for (std::size_t i = 0; i < elem_count; ++i) {
					array.push_back(src[i]);
				}

				insert_at(dst, key, std::move(array));
				break;
			}
		}
	}

private:
	template <typename T>
	static void insert_at(toml::table& dst, std::string_view key, T&& src) {
		auto* current = &dst;
		auto start = key.begin();

		while (start != key.end()) {
			auto end = std::find(start, key.end(), '.');
			std::string_view subkey(start, end);

			if (end == key.end()) {
				current->insert_or_assign(subkey, std::forward<T>(src));
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
};
