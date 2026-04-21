/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "SettingWrapper.hpp"

#include <string_view>

/*==================================================================*/

class HomeDirManager final {
	HomeDirManager(
		std::string_view home_override, std::string_view config_name,
		bool force_portable, std::string_view org, std::string_view app
	) noexcept;

	HomeDirManager(const HomeDirManager&) = delete;
	HomeDirManager& operator=(const HomeDirManager&) = delete;

private:
	std::string m_home_path{};
	std::string m_config_at{};

private:
	void set_home_path(std::string_view home_override, bool portable,
		std::string_view org, std::string_view app) noexcept;
public:
	const auto& get_home_path() const noexcept { return m_home_path; }

private:
	void parse_app_config_file() const noexcept;

public:
	template <typename... Maps> requires (std::same_as<Maps, SettingsMap> && ...)
	void parse_app_config_file(const Maps&... maps) const noexcept {
		(insert_map_into_config(maps), ...);
		parse_app_config_file();
		(update_map_from_config(maps), ...);
	}

private:
	void write_app_config_file() const noexcept;

public:
	template <typename... Maps> requires (std::same_as<Maps, SettingsMap> && ...)
	void write_app_config_file(const Maps&... maps) const noexcept {
		(insert_map_into_config(maps), ...);
		write_app_config_file();
	}

private:
	void insert_map_into_config(const SettingsMap& map) const noexcept;
	void update_map_from_config(const SettingsMap& map) const noexcept;

public:
	static void initialize(
		std::string_view home_override, std::string_view config_name,
		bool force_portable, std::string_view org, std::string_view app
	) noexcept;

public:
	static HomeDirManager* get_instance() noexcept;
};
