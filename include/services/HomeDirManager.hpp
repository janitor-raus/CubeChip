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
		std::string_view overrideHome, std::string_view configName,
		bool forcePortable, std::string_view org, std::string_view app,
		bool& initError
	) noexcept;

	HomeDirManager(const HomeDirManager&) = delete;
	HomeDirManager& operator=(const HomeDirManager&) = delete;

	using GameValidator = bool (*)(
		const char* file_data,
		const std::size_t file_size,
		const std::string& file_exts,
		const std::string& file_sha1
	) noexcept;

	Path m_file_path{};
	std::string m_file_sha1{};

	std::vector<char> // probably should be mmapped later..
		m_file_data{};

	std::vector<Path>
		m_user_directories{};

private:
	static inline
	GameValidator s_validator{};

	static inline
	std::string s_home_path{};

	static inline
	std::string s_config_at{};

private:
	bool set_home_path(std::string_view override, bool portable,
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
		std::string_view overrideHome, std::string_view configName,
		bool forcePortable, std::string_view org, std::string_view app
	) noexcept;

	// returns path pointer to added directory, or nullptr on failure
	auto add_user_directory(const Path& sub, const Path& sys = Path{}) noexcept -> const Path*;

	const auto& get_loaded_file_path() const noexcept { return m_file_path; }
	/***/ auto  get_loaded_file_span() const noexcept { return std::span(m_file_data); }
	/***/ auto  get_loaded_file_data() const noexcept { return m_file_data.data(); }
	/***/ auto  get_loaded_file_size() const noexcept { return m_file_data.size(); }
	const auto& get_loaded_file_sha1() const noexcept { return m_file_sha1; }

	void set_validator_callable(GameValidator func) noexcept { s_validator = std::move(func); }

	void clear_cached_file_data() noexcept;
	bool load_and_validate_file(const Path& gamePath) noexcept;
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
