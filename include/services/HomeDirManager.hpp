/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <span>
#include <vector>
#include <filesystem>

#include "SettingWrapper.hpp"

/*==================================================================*/
	#pragma region HomeDirManager Singleton Class

class HomeDirManager final {
	using Path = std::filesystem::path;

	HomeDirManager(
		std::string_view home_override, std::string_view config_name,
		bool force_portable, std::string_view org, std::string_view app,
		bool& error_on_init
	) noexcept;

	HomeDirManager(const HomeDirManager&) = delete;
	HomeDirManager& operator=(const HomeDirManager&) = delete;

private:
	static inline
	std::string s_home_path{};

	static inline
	std::string s_config_at{};

private:
	bool set_home_path(std::string_view home_override, bool portable,
		std::string_view org, std::string_view app) noexcept;
public:
	static const auto& get_home_path() noexcept { return s_home_path; }

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
	static HomeDirManager* initialize(
		std::string_view home_override, std::string_view config_name,
		bool force_portable, std::string_view org, std::string_view app
	) noexcept;
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
