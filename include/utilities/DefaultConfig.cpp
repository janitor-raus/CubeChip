/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "SimpleFileIO.hpp"
#include "DefaultConfig.hpp"

/*==================================================================*/

auto TomlConfig::writeToFile(
	const toml::table& table,
	const char* filename
) noexcept -> Expected<bool, std::error_code> {
	std::ofstream outFile(filename, std::ios::out);
	if (!outFile) { return ::make_unexpected(std::make_error_code(std::errc::permission_denied)); }

	try { if (outFile << table) { return true; } else { throw std::exception(); } }
	catch (...) { return ::make_unexpected(std::make_error_code(std::errc::io_error)); }
}

auto TomlConfig::parseFromFile(
	const char* filename
) noexcept -> toml::parse_result {
	const auto rawData = ::readFileData(filename ? filename : "");
	const std::string_view tableData = !rawData ? "" :
		std::string_view(rawData.value().data(), rawData.value().size());

	return toml::parse(tableData);
}

/*==================================================================*/

void TomlConfig::safeTableUpdate(toml::table& dst, const toml::table& src) {
	for (auto&& [key, dst_val] : dst) {
		if (const auto* src_val = src.get(key)) {
			if (dst_val.is_table() && src_val->is_table()) {
				safeTableUpdate(*dst_val.as_table(), *src_val->as_table());
			}
			else if (dst_val.is_array() && src_val->is_array()) {
				dst.insert_or_assign(key, *src_val);
			}
			else if (dst_val.is_value() && src_val->is_value()) {
				if (dst_val.type() == src_val->type()) {
					dst.insert_or_assign(key, *src_val);
				}
			}
		}
	}
}

void TomlConfig::safeTableInsert(toml::table& dst, const toml::table& src) {
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
				safeTableInsert(*it->second.as_table(), *src_val.as_table());
			}
		}
	}
}
