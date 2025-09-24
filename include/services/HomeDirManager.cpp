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
	#pragma region HomeDirManager Class

HomeDirManager::HomeDirManager(
	StrV overrideHome, StrV configName,
	bool forcePortable, StrV org, StrV app,
	bool& initError
) noexcept {
	if (!setHomePath(overrideHome, forcePortable, org, app))
		{ initError = true; return; }

	blog.initLogFile("program.log", sHomePath);

	if (configName.empty()) { configName = "settings.toml"; }
	sConfPath = (Path{ sHomePath } / configName).string();
}

void HomeDirManager::triggerFatalError(const char* error) noexcept {
	blog.newEntry(BLOG::FATAL, error);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
		"Fatal Initialization Error", error, nullptr);
}

bool HomeDirManager::isLocationWritable(const char* path) noexcept {
	if (!path) { return false; }
	const auto file{ Path{ path } / "__DELETE_ME__" };
	std::ofstream test{ file };

	if (test.is_open()) {
		test.close();
		const auto result{ fs::remove(file) };
		return result && result.value();
	} else {
		return false;
	}
}

bool HomeDirManager::setHomePath(StrV overrideHome, bool forcePortable, StrV org, StrV app) noexcept {
	if (!overrideHome.empty()) {
		if (isLocationWritable(overrideHome.data())) {
			blog.newEntry(BLOG::INFO, "Home path override successful!");
			sHomePath = overrideHome;
			return true;
		} else {
			triggerFatalError("Home path override failure: cannot write to location!");
			return false;
		}
	}

	if (forcePortable) {
		if (isLocationWritable(::getBasePath())) {
			blog.newEntry(BLOG::INFO, "Forced portable mode successful!");
			sHomePath = ::getBasePath();
			return true;
		} else {
			triggerFatalError("Forced portable mode failure: cannot write to location!");
			return false;
		}
	}

	if (::getBasePath()) {
		const auto fileExists{ fs::exists(Path{ ::getBasePath() } / "portable.txt") };
		if (fileExists && fileExists.value()) {
			if (isLocationWritable(::getBasePath())) {
				sHomePath = ::getBasePath();
				return true;
			} else {
				blog.newEntry(BLOG::ERROR,
					"Portable mode: cannot write to location, falling back to Home path!");
			}
		}
	}

	if (isLocationWritable(::getHomePath(
		org.empty() ? nullptr : org.data(),
		app.empty() ? nullptr : app.data()
	))) {
		sHomePath = ::getHomePath();
		return true;
	} else {
		triggerFatalError("Failed to determine Home path: cannot write to location!");
		return false;
	}
}

void HomeDirManager::parseMainAppConfig() const noexcept {
	if (const auto result{ config::parseFromFile(sConfPath.c_str()) }) {
		config::safeTableUpdate(sMainAppConfig, result.table());
		blog.newEntry(BLOG::INFO,
			"[TOML] App Config found, previous settings loaded!");
	} else {
		blog.newEntry(BLOG::WARN,
			"[TOML] App Config failed to prase! [{}]", result.error().description());
	}
}

void HomeDirManager::writeMainAppConfig() const noexcept {
	if (const auto result{ config::writeToFile(sMainAppConfig, sConfPath.c_str()) }) {
		blog.newEntry(BLOG::INFO,
			"[TOML] App Config written to file successfully!");
	} else {
		blog.newEntry(BLOG::ERROR,
			"[TOML] Failed to write App Config, runtime settings lost! [{}]", result.error().message());
	}
}

void HomeDirManager::insertIntoMainAppConfig(const SettingsMap& map) const noexcept {
	for (auto const& pair : map) {
		const auto& key{ pair.first };
		const auto& ref{ pair.second };
		ref.visit([&](auto* ptr) { config::set(sMainAppConfig, key, *ptr); });
	}
}

void HomeDirManager::updateFromMainAppConfig(const SettingsMap& map) const noexcept {
	for (auto const& pair : map) {
		const auto& key{ pair.first };
		const auto& ref{ pair.second };
		ref.visit([&](auto* ptr) { config::get(sMainAppConfig, key, *ptr); });
	}
}

HomeDirManager* HomeDirManager::initialize(
	StrV overridePath, StrV configName,
	bool forcePortable, StrV org, StrV app
) noexcept {
	static bool initError{};
	static HomeDirManager self(overridePath, configName, forcePortable, org, app, initError);
	return initError ? nullptr : &self;
}

const Path* HomeDirManager::addSystemDir(const Path& sub, const Path& sys) noexcept {
	if (sub.empty()) { return nullptr; }

	const auto newDirPath{ sHomePath / sys / sub };

	const auto it{ std::find_if(EXEC_POLICY(unseq)
		mDirectories.begin(), mDirectories.end(),
		[&newDirPath](const Path& dirEntry) noexcept
			{ return dirEntry == newDirPath; }
	) };

	if (it != mDirectories.end())
		{ return &(*it); }

	if (const auto dirCreated{ fs::create_directories(newDirPath) }) {
		mDirectories.push_back(newDirPath);
		return &mDirectories.back();
	} else {
		blog.newEntry(BLOG::ERROR, "Unable to create directory: \"{}\" [{}]",
			newDirPath.string(), dirCreated.error().message());
		return nullptr;
	}
}

void HomeDirManager::clearCachedFileData() noexcept {
	mFilePath.clear();
	mFileSHA1.clear();
	mFileData.resize(0);
}

bool HomeDirManager::validateGameFile(const Path& gamePath) noexcept {
	const auto fileExists{ fs::is_regular_file(gamePath) };
	if (!fileExists) {
		blog.newEntry(BLOG::WARN, "Path is ineligible: \"{}\" [{}]",
			gamePath.string(), fileExists.error().message());
		return false;
	}
	if (!fileExists.value()) {
		blog.newEntry(BLOG::WARN, "{}: \"{}\"",
			"Path is not a regular file", gamePath.string());
		return false;
	}

	const auto fileSize{ fs::file_size(gamePath) };
	if (!fileSize) {
		blog.newEntry(BLOG::WARN, "Path is ineligible: \"{}\" [{}]",
			gamePath.string(), fileExists.error().message());
		return false;
	}
	if (fileSize.value() == 0) {
		blog.newEntry(BLOG::WARN, "File must not be empty!");
		return false;
	}
	if (fileSize.value() >= MiB(32)) {
		blog.newEntry(BLOG::WARN, "File is too large!");
		return false;
	}

	auto fileData{ ::readFileData(gamePath) };
	if (!fileData) {
		blog.newEntry(BLOG::WARN, "Path is ineligible: \"{}\" [{}]",
			gamePath.string(), fileData.error().message());
		return false;
	} else {
		mFileData = std::move(fileData.value());
	}

	const auto tempSHA1{ SHA1::from_data(getFileData(), getFileSize()) };
	blog.newEntry(BLOG::INFO, "File SHA1: {}", tempSHA1);

	if (sCheckGame(
		getFileData(), getFileSize(),
		gamePath.extension().string(), tempSHA1
	)) {
		mFilePath = gamePath;
		mFileSHA1 = tempSHA1;
		return true;
	} else {
		return false;
	}
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
