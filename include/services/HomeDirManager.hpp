/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <span>
#include <vector>

#include "Typedefs.hpp"
#include "SettingWrapper.hpp"

#include "AtomSharedPtr.hpp"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

/*==================================================================*/
	#pragma region HomeDirManager Singleton Class

class HomeDirManager final {
	HomeDirManager(
		StrV overrideHome, StrV configName,
		bool forcePortable, StrV org, StrV app,
		bool& initError
	) noexcept;
	HomeDirManager(const HomeDirManager&) = delete;
	HomeDirManager& operator=(const HomeDirManager&) = delete;

	using GameValidator = bool (*)(
		const char* fileData,
		const size_type   fileSize,
		const Str&  fileExts,
		const Str&  fileSHA1
	) noexcept;

	Path mFilePath{};
	Str  mFileSHA1{};

	std::vector<char>
		mFileData{};

	std::vector<Path>
		mDirectories{};

	static inline AtomSharedPtr<Str>
		mProbableFile{ nullptr };

public:
	[[nodiscard]]
	static auto getProbableFile() noexcept
		{ return mProbableFile.exchange(nullptr, mo::relaxed); }

	static void setProbableFile(StrV file) noexcept
		{ mProbableFile.store(std::make_shared<Str>(file), mo::relaxed); }

	static void probableFileCallback(void*, const char* const* filelist, int) noexcept
		{ if (filelist && filelist[0]) { setProbableFile(filelist[0]); } }

	static auto getHomeDirectoryURL() noexcept
		{ return "file:///" + sHomePath; }

private:
	static inline GameValidator sCheckGame{};
	static inline toml::table   sMainAppConfig{};

private:
	static inline Str sHomePath{};
	static inline Str sConfPath{};

	static void triggerFatalError(const char* error) noexcept;
	static bool isLocationWritable(const char* path) noexcept;

	bool setHomePath(StrV override, bool portable, StrV org, StrV app) noexcept;

public:
	void parseMainAppConfig() const noexcept;

	template <typename... Maps>
		requires (std::same_as<Maps, SettingsMap> && ...)
	void parseMainAppConfig(const Maps&... maps) const noexcept {
		(insertIntoMainAppConfig(maps), ...);
		parseMainAppConfig();
		(updateFromMainAppConfig(maps), ...);
	}

	void writeMainAppConfig() const noexcept;

	template <typename... Maps>
		requires (std::same_as<Maps, SettingsMap> && ...)
	void writeMainAppConfig(const Maps&... maps) const noexcept {
		(insertIntoMainAppConfig(maps), ...);
		writeMainAppConfig();
	}

private:
	void insertIntoMainAppConfig(const SettingsMap& map) const noexcept;
	void updateFromMainAppConfig(const SettingsMap& map) const noexcept;

public:
	static HomeDirManager* initialize(
		StrV overrideHome, StrV configName,
		bool forcePortable, StrV org, StrV app
	) noexcept;

	const Path* addSystemDir(const Path& sub, const Path& sys = Path{}) noexcept;

	auto getFullPath() const noexcept { return mFilePath; }
	auto getFilePath() const noexcept { return mFilePath.string(); }
	auto getFileName() const noexcept { return mFilePath.filename().string(); }
	auto getFileStem() const noexcept { return mFilePath.stem().string(); }
	auto getFileExts() const noexcept { return mFilePath.extension().string(); }
	auto getFileSpan() const noexcept { return std::span{ mFileData }; }
	auto getFileSize() const noexcept { return mFileData.size(); }
	auto getFileData() const noexcept { return mFileData.data(); }
	auto getFileSHA1() const noexcept { return mFileSHA1; }

	void setValidator(GameValidator func) noexcept { sCheckGame = func; }

	void clearCachedFileData() noexcept;
	bool validateGameFile(const Path& gamePath) noexcept;
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
