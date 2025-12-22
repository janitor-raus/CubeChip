/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>

#include "Typedefs.hpp"
#include "SimpleMRU.hpp"
#include "SettingWrapper.hpp"

/*==================================================================*/

#ifndef PROJECT_VERSION_MAJOR_I
	#define PROJECT_VERSION_MAJOR_I 0
#endif
#ifndef PROJECT_VERSION_MINOR_I
	#define PROJECT_VERSION_MINOR_I 0
#endif
#ifndef PROJECT_VERSION_PATCH_I
	#define PROJECT_VERSION_PATCH_I 0
#endif
#ifndef PROJECT_VERSION_TWEAK_I
	#define PROJECT_VERSION_TWEAK_I 0
#endif
#ifndef PROJECT_VERSION_MAJOR
	#define PROJECT_VERSION_MAJOR "0"
#endif
#ifndef PROJECT_VERSION_MINOR
	#define PROJECT_VERSION_MINOR "0"
#endif
#ifndef PROJECT_VERSION_PATCH
	#define PROJECT_VERSION_PATCH "0"
#endif
#ifndef PROJECT_VERSION_TWEAK
	#define PROJECT_VERSION_TWEAK "0"
#endif
#ifndef PROJECT_VERSION_GHASH
	#define PROJECT_VERSION_GHASH "unknown"
#endif
#ifndef PROJECT_VERSION_WITH_DATE
	#define PROJECT_VERSION_WITH_DATE "0.0.0.0"
#endif
#ifndef PROJECT_VERSION_WITH_HASH
	#define PROJECT_VERSION_WITH_HASH "0.0.0.0.unknown"
#endif
#ifndef PROJECT_NAME
	#define PROJECT_NAME "CubeChip?"
#endif

/*==================================================================*/

struct ProjectVersion {
	constexpr static inline auto major_i = PROJECT_VERSION_MAJOR_I;
	constexpr static inline auto minor_i = PROJECT_VERSION_MINOR_I;
	constexpr static inline auto patch_i = PROJECT_VERSION_PATCH_I;
	constexpr static inline auto tweak_i = PROJECT_VERSION_TWEAK_I;

	constexpr static inline auto* major = PROJECT_VERSION_MAJOR;
	constexpr static inline auto* minor = PROJECT_VERSION_MINOR;
	constexpr static inline auto* patch = PROJECT_VERSION_PATCH;
	constexpr static inline auto* tweak = PROJECT_VERSION_TWEAK;

	constexpr static inline auto* ghash = PROJECT_VERSION_GHASH;

	constexpr static inline auto* with_date = PROJECT_VERSION_WITH_DATE;
	constexpr static inline auto* with_hash = PROJECT_VERSION_WITH_HASH;
};

constexpr static inline ProjectVersion AppVer{};

#if !defined(NDEBUG) || defined(DEBUG)
	constexpr auto* AppName = PROJECT_NAME " [DEBUG]";
#else
	constexpr auto* AppName = PROJECT_NAME;
#endif
	constexpr auto* OrgName = "";

/*==================================================================*/

class HomeDirManager;
class GlobalAudioBase;
class BasicVideoSpec;
class SystemInterface;

/*==================================================================*/

class FrontendHost final {
	FrontendHost(const Path&) noexcept;

	FrontendHost(const FrontendHost&) = delete;
	FrontendHost& operator=(const FrontendHost&) = delete;

	struct StopSystemThread {
		void operator()(SystemInterface*) noexcept;
	};
	using SystemCore = std::unique_ptr
		<SystemInterface, StopSystemThread>;

	SystemCore mSystemCore;

private:


	static inline SimpleMRU<FileItem, 5> m_file_mru;

public:
	static inline HomeDirManager*  HDM{};
	static inline GlobalAudioBase* GAB{};
	static inline BasicVideoSpec*  BVS{};

	struct Settings {
		float ui_scale = 1.5f;

		SettingsMap map() noexcept;
	};

private:
	auto exportSettings() const noexcept -> Settings;

private:
	static inline bool mToggleOSD{};
	static inline bool mUnlimited{};

	void handleHotkeyActions();
	void initializeInterface() noexcept;

	void toggleSystemLimiter() noexcept;
	void toggleSystemOSD()     noexcept;
	void toggleSystemHidden(bool state) noexcept;

	void discardCore();
	void replaceCore();

public:
	static auto* initialize(const Path& filepath) noexcept {
		static FrontendHost self(filepath);
		return &self;
	}

	static bool initApplication(StrV overrideHome, StrV configName, bool forcePortable) noexcept;

	void quitApplication() noexcept;
	void loadGameFile(const Path&);

	s32  processEvents(void* event) noexcept;
	s32  processFrame();
};

/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
