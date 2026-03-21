/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDirManager.hpp"

#include "SimpleFileIO.hpp"
#include "BasicLogger.hpp"
#include "DefaultConfig.hpp"
#include "PathGetters.hpp"

#include <SDL3/SDL_messagebox.h>
#include <fmt/format.h>

/*==================================================================*/

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

static toml::table s_config_model{};

/*==================================================================*/

template <typename... T>
static void println_fatal(fmt::format_string<T...> fmt_str, T&&... args) noexcept {
	std::string message = fmt::format(fmt_str, std::forward<T>(args)...);
	fmt::println(stderr, "{}", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
		"Fatal Initialization Error", message.c_str(), nullptr);
}

/*==================================================================*/

HomeDirManager::HomeDirManager(
	std::string_view home_override, std::string_view config_name,
	bool force_portable, std::string_view org, std::string_view app
) noexcept {
	set_home_path(home_override, force_portable, org, app);
	if (s_home_path.empty()) { return; }

	if (config_name.empty()) { config_name = "settings.toml"; }
	s_config_at = (fs::Path(s_home_path) / config_name).string();
}

HomeDirManager* HomeDirManager::initialize(
	std::string_view home_override, std::string_view config_name,
	bool force_portable, std::string_view org, std::string_view app
) noexcept {
	static HomeDirManager self(home_override, config_name, force_portable, org, app);
	return s_home_path.empty() ? nullptr : &self;
}

/*==================================================================*/

void HomeDirManager::set_home_path(
	std::string_view home_override, bool force_portable,
	std::string_view org, std::string_view app
) noexcept {
	if (!home_override.empty()) {
		const auto writable_dir = fs::is_writable_directory(home_override);
		if (writable_dir.value_or(false)) {
			fmt::println("Home path (--homedir) has been successfully "
				"re-routed to '{}'.", (s_home_path = home_override));
			return;
		} else {
			::println_fatal("Failed to re-route Home path (--homedir) to '{}': {}", home_override,
				writable_dir.error() ? writable_dir.error().message() : "not a directory or doesn't exist!");
			return;
		}
	}

	if (force_portable) {
		const auto writable_dir = fs::is_writable_directory(::get_base_path());
		if (writable_dir.value_or(false)) {
			fmt::println("Home path (--portable) has been successfully "
				"re-routed to '{}'.", (s_home_path = ::get_base_path()));
			return;
		} else {
			::println_fatal("Failed to re-route Home path (--portable) to '{}': {}", ::get_base_path(),
				writable_dir.error() ? writable_dir.error().message() : "cannot write to directory!");
			return;
		}
	}

	if (!::get_base_path().empty()) {
		if (fs::exists(fs::Path(::get_base_path()) / "portable.txt").value_or(false)) {
			const auto writable_dir = fs::is_writable_directory(::get_base_path());
			if (writable_dir.value_or(false)) {
				fmt::println("Home path (portable.txt) has been successfully "
					"re-routed to '{}'.", (s_home_path = ::get_base_path()));
				return;
			} else {
				::println_fatal("Failed to re-route Home path (portable.txt) to '{}': {}"
					"\nIgnoring error and falling back to default Home path detection!", ::get_base_path(),
					writable_dir.error() ? writable_dir.error().message() : "cannot write to directory!");
			}
		}
	}

	const auto writable_dir = fs::is_writable_directory(::get_home_path(
		org.empty() ? nullptr : org.data(),
		app.empty() ? nullptr : app.data()
	));
	if (writable_dir.value_or(false)) {
		fmt::println("Home path has been successfully "
			"set to '{}'.", (s_home_path = ::get_home_path()));
		return;
	} else {
		::println_fatal("Failed to determine if '{}' is writable: {}", ::get_home_path(),
			writable_dir.error() ? writable_dir.error().message() : "not a directory or doesn't exist!");
		return;
	}
}

/*==================================================================*/

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
/*==================================================================*/
