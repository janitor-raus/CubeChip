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

// DEPRECTATED, FOR FUTURE REFERENCE ONLY
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
	BytePusher,
	gb, gbc // GAMEBOY/GAMEBOY COLOR
};

/*==================================================================*/

class SystemInterface;

// simple callable to construct a Core instance, returning SystemInterface*
using CoreConstructor = SystemInterface* (*)();
// simple callable to statically check a game's compatibility with the Core
using GameFileTester  = bool (*)(const char*, std::size_t);
// plain vector of file extensions usually associated with a Core or System
using KnownExtensions = std::vector<std::string>;

// simple struct of a Core's basic details -- very wip
struct CoreDetails {
	CoreConstructor construct_core{};
	GameFileTester  test_game_file{};
	KnownExtensions known_extensions{};

	std::string core_name{};
	std::string core_desc{};

	void clear() noexcept {
		construct_core = nullptr;
		test_game_file = nullptr;
		known_extensions.clear();
		core_name.clear();
		core_desc.clear();
	}
};

using CoreRegList = std::vector<CoreDetails>;
using Json        = nlohmann::json;

/*==================================================================*/

#define REGISTER_CORE(CoreType, ...) \
static auto CONCAT_TOKENS(sCoreRegID_, __COUNTER__) = \
	CoreRegistry::register_new_core( \
		[]() -> SystemInterface* { \
			return new (std::align_val_t(HDIS), std::nothrow) CoreType(); \
		}, CoreType::validate_program, { __VA_ARGS__ } \
	);

/*==================================================================*/

class CoreRegistry {
	using RegistryKey = std::string;
	using Registrations = std::unordered_map<RegistryKey, CoreRegList>;

	static auto& get_registry_map() noexcept {
		static Registrations s_core_registry{};
		return s_core_registry;
	}

	static inline CoreRegList s_potential_cores{};
	static inline CoreDetails s_selected_core{};

	static Json s_game_database;
	static Json s_custom_core_cfg;

	CoreRegistry()                               = delete;
	CoreRegistry(const CoreRegistry&)            = delete;
	CoreRegistry& operator=(const CoreRegistry&) = delete;

	static bool validate_by_sha1_hash(const char* fileData, std::size_t fileSize, const std::string& fileSHA1) noexcept;
	static bool validate_by_extension(const char* fileData, std::size_t fileSize, const std::string& fileType) noexcept;

public:
	static bool validate_game_file(const char* fileData, std::size_t fileSize, const std::string& fileType, const std::string& fileSHA1) noexcept;

	static void load_game_database(std::string_view db_file_path = {}) noexcept;

	static bool register_new_core(CoreConstructor&& ctor,
		GameFileTester&& tester, KnownExtensions exts) noexcept;

	[[nodiscard]] static
	SystemInterface* construct_new_core(std::size_t idx = 0) noexcept;

	/*==================================================================*/

	static void clear_eligible_cores() noexcept {
		s_potential_cores.clear();
		s_selected_core.clear();
	}
	static void clear_current_core() noexcept {
		s_selected_core.clear();
	}

	[[nodiscard]]
	static const auto& get_eligible_core_list() noexcept { return s_potential_cores; }
	[[nodiscard]]
	static const auto& get_selected_core() noexcept { return s_selected_core; }
};
