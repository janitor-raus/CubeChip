/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string_view>
#include <algorithm>
#include <toml++/toml.hpp>

/*==================================================================*/

struct TomlConfig {
	/**
	 * @brief Safely updates existing entries in a TOML table from another table.
	 *
	 * Iterates over keys already present in @p dst and updates them from @p src
	 * only when the key exists in both tables. Type mismatches are ignored!
	 *
	 * Rules:
	 * - Tables are merged recursively.
	 * - Arrays are replaced wholesale.
	 * - Values are replaced regardless of their TOML types.
	 * - Keys present only in @p src are ignored.
	 *
	 * @param dst Destination table to be updated in-place.
	 * @param src Source table providing updated values.
	 */
	static void merge_overwrite(toml::table& dst, const toml::table& src);

	/**
	 * @brief Safely inserts missing entries from one TOML table into another.
	 *
	 * Iterates over keys in @p src and inserts them into @p dst only if the key
	 * does not already exist in @p dst.
	 *
	 * Rules:
	 * - Missing keys are inserted as-is.
	 * - Tables are deep-copied on insert.
	 * - If a key exists in both tables and both values are tables,
	 *     insertion recurses into the nested tables.
	 * - Existing non-table values are never overwritten.
	 *
	 * @param dst Destination table to be modified in-place.
	 * @param src Source table providing default values.
	 */
	static void merge_fill_only(toml::table& dst, const toml::table& src);

	/*==================================================================*/

	// Writes a TOML file and returns false on success, true if an error occurred otherwise.
	static bool write_into_file(const toml::table& table, const char* filename) noexcept;

	// Reads a TOML file and returns a parse_result, which can be checked for success or failure.
	static auto parse_from_file(const char* filename) noexcept -> toml::parse_result;

	/*==================================================================*/

	template <typename T>
	static void get(const toml::table& src, std::string_view key, T* dst, std::size_t elem_count = 1) noexcept {
		switch (elem_count) {
			case 0: case 1:
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
	static bool try_get(const toml::table& src, std::string_view key, T* dst, std::size_t elem_count = 1) noexcept {
		const auto node = src.at_path(key);
		if (!node) { return false; }

		switch (elem_count) {
			case 0: case 1:
				*dst = node.value_or(T());
				return true;

			default:
				if (auto* array = node.as_array()) {
					const auto limit = std::min(elem_count, array->size());

					for (std::size_t i = 0; i < limit; ++i) {
						dst[i] = (*array)[i].value_or(T());
					}
					return limit > 0;
				}
				return false;
		}
	}

	template <typename T>
	static void set(toml::table& dst, std::string_view key, T&& src) noexcept {
		insert_at(dst, key, std::forward<T>(src));
	}

	template <typename T>
	static bool try_set(toml::table& dst, std::string_view key, T&& src) noexcept {
		if (dst.at_path(key)) {
			return false;
		} else {
			insert_at(dst, key, std::forward<T>(src));
			return true;
		}
	}

	template <typename T>
	static void set(toml::table& dst, std::string_view key, const T* src, std::size_t elem_count = 1) noexcept {
		switch (elem_count) {
			case 0: case 1:
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

	template <typename T>
	static bool try_set(toml::table& dst, std::string_view key, const T* src, std::size_t elem_count = 1) noexcept {
		if (dst.at_path(key)) {
			return false;
		} else {
			set(dst, key, src, elem_count);
			return true;
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

/*==================================================================*/

class ConfigScope {
	toml::table& m_table;

	static auto& navigate_or_create(toml::table& root, std::string_view path) noexcept {
		auto* current = &root;
		auto start = path.begin();

		while (start != path.end()) {
			auto end = std::find(start, path.end(), '.');
			std::string_view subkey(start, end);

			if (!current->contains(subkey)) {
				current->insert(subkey, toml::table{});
			}
			current = current->get(subkey)->as_table();
			start = (end == path.end()) ? end : end + 1;
		}
		return *current;
	}

public:
	ConfigScope(toml::table& root, std::string_view path) noexcept
		: m_table(navigate_or_create(root, path))
	{}

	template <typename T>
	void set(std::string_view subkey, T&& src) noexcept {
		TomlConfig::set(m_table, subkey, std::forward<T>(src));
	}

	template <typename T>
	void set(std::string_view subkey, const T* src, std::size_t count = 1) noexcept {
		TomlConfig::set(m_table, subkey, src, count);
	}

	template <typename T>
	T get(std::string_view subkey, T fallback = T()) const noexcept {
		T dst = fallback;
		TomlConfig::get(m_table, subkey, &dst, 1);
		return dst;
	}
};
