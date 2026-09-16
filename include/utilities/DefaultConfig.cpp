/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <fstream>

#include "DefaultConfig.hpp"

/*==================================================================*/

bool TomlConfig::write_into_file(const toml::table& table, const char* filename) noexcept {
	std::ofstream output_file(filename, std::ios::out);
	if (!output_file) { return true; }

	output_file << table;
	if (!output_file) { return true; }

	return false;
}

auto TomlConfig::parse_from_file(const char* filename) noexcept -> toml::parse_result {
	return toml::parse_file(filename ? filename : "");
}

/*==================================================================*/

void TomlConfig::merge_overwrite(toml::table& dst, const toml::table& src) {
	for (auto&& [key, dst_val] : dst) {
		if (const auto* src_val = src.get(key)) {
			if (dst_val.is_table() && src_val->is_table()) {
				merge_overwrite(*dst_val.as_table(), *src_val->as_table());
			}
			else if (dst_val.is_array() && src_val->is_array()) {
				dst.insert_or_assign(key, *src_val);
			}
			else if (dst_val.is_value() && src_val->is_value()) {
				dst.insert_or_assign(key, *src_val);
			}
		}
	}
}

void TomlConfig::merge_fill_only(toml::table& dst, const toml::table& src) {
	for (auto&& [key, src_val] : src) {
		if (auto it = dst.find(key); it == dst.end()) {
			if (src_val.is_table()) {
				dst.insert(key, *src_val.as_table());
			} else {
				dst.insert(key, src_val);
			}
		}
		else {
			if (src_val.is_table() && it->second.is_table()) {
				merge_fill_only(*it->second.as_table(), *src_val.as_table());
			}
		}
	}
}
