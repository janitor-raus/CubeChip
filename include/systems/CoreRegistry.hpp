/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <new>
#include <vector>
#include <unordered_map>

#include "Macros.hpp"
#include "Typedefs.hpp"
#include "HDIS_HCIS.hpp"
#include "nlohmann/json_fwd.hpp"

/*==================================================================*/

enum class GameFileType {
	c2x, // CHIP-8X 2-page
	c4x, // CHIP-8X 4-page
	c8x, // CHIP-8X
	c8e, // CHIP-8E
	c2h, // CHIP-8 (HIRES) 2-page
	c4h, // CHIP-8 (HIRES) 4-page
	c8h, // CHIP-8 (HIRES) 2-page patched
	ch8, // CHIP-8
	sc8, // SUPERCHIP
	mc8, // MEGACHIP
	gc8, // GIGACHIP
	xo8, // XO-CHIP
	hwc, // HYPERWAVE-CHIP
	bnc,
	BytePusher,
	gb, gbc // GAMEBOY/GAMEBOY COLOR
};

/*==================================================================*/

class SystemInterface;

using CoreConstructor = SystemInterface* (*)();
using ProgramTester   = bool (*)(const char*, size_type);
using FileExtList     = std::vector<Str>;

struct CoreDetails {
	CoreConstructor constructCore{};
	ProgramTester   testProgram{};
	FileExtList     fileExtensions{};

	Str coreName{};
	Str coreDesc{};

	void clear() noexcept {
		constructCore = nullptr;
		testProgram   = nullptr;
		fileExtensions.clear();
		coreName.clear();
		coreDesc.clear();
	}
};

using CoreRegList = std::vector<CoreDetails>;
using Json        = nlohmann::json;

/*==================================================================*/

#define REGISTER_CORE(CoreType, ...) \
static auto CONCAT_TOKENS(sCoreRegID_, __COUNTER__) = \
	CoreRegistry::registerCore( \
		[]() -> SystemInterface* { \
			return new (std::align_val_t(HDIS), std::nothrow) CoreType(); \
		}, CoreType::validateProgram, { __VA_ARGS__ } \
	);

/*==================================================================*/

class CoreRegistry {
	using Registrations = std::unordered_map<Str, CoreRegList>;
	static Registrations& getRegistry() noexcept {
		static Registrations sRegistry{};
		return sRegistry;
	}

	static inline CoreRegList   sEligible{};
	static inline CoreDetails   sCurrentCore{};

	static Json sProgramDB;
	static Json sCoreConfig;

	CoreRegistry()                               = delete;
	CoreRegistry(const CoreRegistry&)            = delete;
	CoreRegistry& operator=(const CoreRegistry&) = delete;

	static bool validateProgramByHash(const char* fileData, size_type fileSize, const Str& fileSHA1) noexcept;
	static bool validateProgramByType(const char* fileData, size_type fileSize, const Str& fileType) noexcept;

public:
	static bool validateProgram(const char* fileData, size_type fileSize, const Str& fileType, const Str& fileSHA1) noexcept;

public:
	static void loadProgramDB(const Path& dbPath = {}) noexcept;
private:
	static bool loadJsonFromFile(const Path& path, Json& output) noexcept;

	/*==================================================================*/

public:
	static bool registerCore(CoreConstructor&& ctor, ProgramTester&& tester, FileExtList exts) noexcept;

	static const CoreRegList* findEligibleCores(const Str& ext) noexcept;

	[[nodiscard]]
	static SystemInterface* constructCore(size_type idx = 0) noexcept;

	/*==================================================================*/

	static void clearEligibleCores() noexcept {
		sEligible.clear();
		sCurrentCore.clear();
	}
	static void clearCurrentCore() noexcept {
		sCurrentCore.clear();
	}

	[[nodiscard]]
	static const auto& getEligibleCores() noexcept { return sEligible; }
	[[nodiscard]]
	static const auto& getCurrentCore() noexcept { return sCurrentCore; }
};
