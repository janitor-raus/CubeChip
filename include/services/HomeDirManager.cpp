/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDirManager.hpp"

#include "EzMaths.hpp"
#include "SimpleFileIO.hpp"
#include "BasicLogger.hpp"
#include "DefaultConfig.hpp"
#include "PathGetters.hpp"
#include "ExecPolicy.hpp"

#include <SDL3/SDL_messagebox.h>

/*==================================================================*/

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

static toml::table s_config_model{};

/*==================================================================*/

static void trigger_fatal_error(const char* error) noexcept {
	blog.fatal(error);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
		"Fatal Initialization Error", error, nullptr);
}

static bool is_location_writable(const char* path) noexcept {
	if (!path) { return false; }
	const auto file = fs::Path(path) / "__DELETE_ME__";
	std::ofstream test(file);

	if (test.is_open()) {
		test.close();
		const auto result = fs::remove(file);
		return result && result.value();
	} else {
		return false;
	}
}

/*==================================================================*/
	#pragma region HomeDirManager Class

HomeDirManager::HomeDirManager(
	std::string_view home_override, std::string_view config_name,
	bool force_portable, std::string_view org, std::string_view app,
	bool& error_on_init
) noexcept {
	if (!set_home_path(home_override, force_portable, org, app))
		{
		error_on_init = true; return; }

	if (config_name.empty()) { config_name = "settings.toml"; }
	s_config_at = (Path(s_home_path) / config_name).string();
}

bool HomeDirManager::set_home_path(
	std::string_view home_override, bool force_portable,
	std::string_view org, std::string_view app) noexcept
{
	if (!home_override.empty()) {
		if (::is_location_writable(home_override.data())) {
			blog.info("Home path override successful!");
			s_home_path = home_override;
			return true;
		} else {
			::trigger_fatal_error("Home path override failure: cannot write to location!");
			return false;
		}
	}

	if (force_portable) {
		if (::is_location_writable(::get_base_path())) {
			blog.info("Forced portable mode successful!");
			s_home_path = ::get_base_path();
			return true;
		} else {
			::trigger_fatal_error("Forced portable mode failure: cannot write to location!");
			return false;
		}
	}

	if (::get_base_path()) {
		const auto fileExists = fs::exists(Path(::get_base_path()) / "portable.txt");
		if (fileExists && fileExists.value()) {
			if (::is_location_writable(::get_base_path())) {
				s_home_path = ::get_base_path();
				return true;
			} else {
				blog.error("Portable mode: cannot write to location, falling back to Home path!");
			}
		}
	}

	if (::is_location_writable(::get_home_path(
		org.empty() ? nullptr : org.data(),
		app.empty() ? nullptr : app.data()
	))) {
		s_home_path = ::get_home_path();
		return true;
	} else {
		::trigger_fatal_error("Failed to determine Home path: cannot write to location!");
		return false;
	}
}

void HomeDirManager::parse_app_config_file() const noexcept {
	if (const auto result = TomlConfig::parse_from_file(s_config_at.c_str())) {
		TomlConfig::update_existing_table_contents(s_config_model, result.table());
		blog.info("[TOML] App Config found, previous settings loaded!");
	} else {
		blog.warn("[TOML] App Config failed to parse!"
			" [{}]", result.error().description());
	}
}

void HomeDirManager::write_app_config_file() const noexcept {
	if (const auto result = TomlConfig::write_into_file(s_config_model, s_config_at.c_str())) {
		blog.info("[TOML] App Config written to file successfully!");
	} else {
		blog.error("[TOML] Failed to write App Config, runtime settings lost!"
			" [{}]", result.error().message());
	}
}

void HomeDirManager::insert_map_into_config(const SettingsMap& map) const noexcept {
	for (const auto& [key, setting] : map) {
		setting.visit([&](auto* ptr) noexcept {
			TomlConfig::set(s_config_model, key, ptr, setting.elem_count());
		});
	}
}

void HomeDirManager::update_map_from_config(const SettingsMap& map) const noexcept {
	for (const auto& [key, setting] : map) {
		setting.visit([&](auto* ptr) noexcept {
			TomlConfig::get(s_config_model, key, ptr, setting.elem_count());
		});
	}
}

HomeDirManager* HomeDirManager::initialize(
	std::string_view home_override, std::string_view config_name,
	bool force_portable, std::string_view org, std::string_view app
) noexcept {
	static bool error_on_init{};
	static HomeDirManager self(home_override, config_name, force_portable, org, app, error_on_init);
	return error_on_init ? nullptr : &self;
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
