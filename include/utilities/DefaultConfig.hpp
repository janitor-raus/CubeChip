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
	/**
	 * @brief Safely updates existing entries in a TOML table from another table.
	 *
	 * Iterates over keys already present in @p dst and updates them from @p src
	 * only when the key exists in both tables and the node types are compatible.
	 *
	 * Rules:
	 * - Tables are merged recursively.
	 * - Arrays are replaced wholesale.
	 * - Values are replaced only if their TOML types match.
	 * - Keys present only in @p src are ignored.
	 *
	 * @param dst Destination table to be updated in-place.
	 * @param src Source table providing updated values.
	 */
	static void update_existing_table_contents(toml::table& dst, const toml::table& src);

	/**
	 * @brief Safely inserts missing entries from one TOML table into another.
	 *
	 * Iterates over keys in @p src and inserts them into @p dst only if the key
	 * does not already exist in @p dst.
	 *
	 * Rules:
	 * - Missing keys are inserted as-is.
	 * - Tables are deep-copied on insert.
	 * - If a key exists in both tables and both values are tables, insertion
	 *   recurses into the nested tables.
	 * - Existing non-table values are never overwritten.
	 *
	 * @param dst Destination table to be modified in-place.
	 * @param src Source table providing default values.
	 */
	static void insert_missing_table_entries(toml::table& dst, const toml::table& src);

	static auto write_into_file(const toml::table& table, const char* filename) noexcept
		-> Expected<bool, std::error_code>;

	static auto parse_from_file(const char* filename) noexcept
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
