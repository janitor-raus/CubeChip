/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDirManager.hpp"

#include "SHA1.hpp"
#include "SimpleFileIO.hpp"
#include "BasicLogger.hpp"
#include "DefaultConfig.hpp"
#include "PathGetters.hpp"
#include "ExecPolicy.hpp"

#include <algorithm>

#include <SDL3/SDL_messagebox.h>

/*==================================================================*/

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

static toml::table s_config_model{};

/*==================================================================*/

static void trigger_fatal_error(const char* error) noexcept {
	blog.newEntry<BLOG::FTL>(error);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
		"Fatal Initialization Error", error, nullptr);
}

static bool is_location_writable(const char* path) noexcept {
	if (!path) { return false; }
	const auto file = Path(path) / "__DELETE_ME__";
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
	std::string_view overrideHome, std::string_view configName,
	bool forcePortable, std::string_view org, std::string_view app,
	bool& initError
) noexcept {
	if (!set_home_path(overrideHome, forcePortable, org, app))
		{ initError = true; return; }

	if (configName.empty()) { configName = "settings.toml"; }
	s_config_at = (Path(s_home_path) / configName).string();
}

bool HomeDirManager::set_home_path(
	std::string_view overrideHome, bool forcePortable,
	std::string_view org, std::string_view app) noexcept
{
	if (!overrideHome.empty()) {
		if (::is_location_writable(overrideHome.data())) {
			blog.newEntry<BLOG::INF>("Home path override successful!");
			s_home_path = overrideHome;
			return true;
		} else {
			::trigger_fatal_error("Home path override failure: cannot write to location!");
			return false;
		}
	}

	if (forcePortable) {
		if (::is_location_writable(::get_base_path())) {
			blog.newEntry<BLOG::INF>("Forced portable mode successful!");
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
				blog.newEntry<BLOG::ERR>(
					"Portable mode: cannot write to location, falling back to Home path!");
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
	if (const auto result = TomlConfig::parseFromFile(s_config_at.c_str())) {
		TomlConfig::safeTableUpdate(s_config_model, result.table());
		blog.newEntry<BLOG::INF>(
			"[TOML] App Config found, previous settings loaded!");
	} else {
		blog.newEntry<BLOG::WRN>(
			"[TOML] App Config failed to parse! [{}]", result.error().description());
	}
}

void HomeDirManager::write_app_config_file() const noexcept {
	if (const auto result = TomlConfig::writeToFile(s_config_model, s_config_at.c_str())) {
		blog.newEntry<BLOG::INF>(
			"[TOML] App Config written to file successfully!");
	} else {
		blog.newEntry<BLOG::ERR>(
			"[TOML] Failed to write App Config, runtime settings lost! [{}]", result.error().message());
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
	std::string_view overridePath, std::string_view configName,
	bool forcePortable, std::string_view org, std::string_view app
) noexcept {
	static bool initError{};
	static HomeDirManager self(overridePath, configName, forcePortable, org, app, initError);
	return initError ? nullptr : &self;
}

const Path* HomeDirManager::add_user_directory(const Path& sub, const Path& sys) noexcept {
	if (sub.empty()) { return nullptr; }

	const auto newDirPath = s_home_path / sys / sub;

	const auto it = std::find_if(EXEC_POLICY(unseq)
		m_user_directories.begin(), m_user_directories.end(),
		[&](const Path& dirEntry) noexcept { return dirEntry == newDirPath; }
	);

	if (it != m_user_directories.end())
		{ return &(*it); }

	if (const auto dirCreated = fs::create_directories(newDirPath)) {
		m_user_directories.push_back(newDirPath);
		return &m_user_directories.back();
	} else {
		blog.newEntry<BLOG::ERR>("Unable to create directory: \"{}\" [{}]",
			newDirPath.string(), dirCreated.error().message());
		return nullptr;
	}
}

void HomeDirManager::clear_cached_file_data() noexcept {
	m_file_path.clear();
	m_file_sha1.clear();
	m_file_data.resize(0);
}

bool HomeDirManager::load_and_validate_file(const Path& gamePath) noexcept {
	const auto fileExists = fs::is_regular_file(gamePath);
	if (!fileExists) {
		blog.newEntry<BLOG::WRN>("Path is ineligible: \"{}\" [{}]",
			gamePath.string(), fileExists.error().message());
		return false;
	}
	if (!fileExists.value()) {
		blog.newEntry<BLOG::WRN>("{}: \"{}\"",
			"Path is not a regular file", gamePath.string());
		return false;
	}

	const auto fileSize = fs::file_size(gamePath);
	if (!fileSize) {
		blog.newEntry<BLOG::WRN>("Path is ineligible: \"{}\" [{}]",
			gamePath.string(), fileExists.error().message());
		return false;
	}
	if (fileSize.value() == 0) {
		blog.newEntry<BLOG::WRN>("File must not be empty!");
		return false;
	}
	if (fileSize.value() >= 32_MiB) {
		blog.newEntry<BLOG::WRN>("File is too large!");
		return false;
	}

	auto fileData = ::readFileData(gamePath);
	if (!fileData) {
		blog.newEntry<BLOG::WRN>("Path is ineligible: \"{}\" [{}]",
			gamePath.string(), fileData.error().message());
		return false;
	} else {
		m_file_data = std::move(fileData.value());
	}

	const auto tempSHA1 = SHA1::from_data(m_file_data.data(), m_file_data.size());
	blog.newEntry<BLOG::INF>("File SHA1: {}", tempSHA1);

	if (s_validator(m_file_data.data(), m_file_data.size(),
		gamePath.extension().string(), tempSHA1))
	{
		m_file_path = gamePath;
		m_file_sha1 = tempSHA1;
		return true;
	} else {
		return false;
	}
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
