/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "nlohmann/json.hpp"
#include "BasicLogger.hpp"
#include "PathGetters.hpp"
#include "SimpleFileIO.hpp"
#include "CoreRegistry.hpp"

/*==================================================================*/

Json CoreRegistry::s_game_database{};
Json CoreRegistry::s_custom_core_cfg{};

static bool loadJsonFromFile(const Path& path, Json& output) noexcept {
	if (auto jsonData = ::readFileData(path)) {
		try {
			output = Json::parse(jsonData->begin(), jsonData->end());
			return true;
		} catch (const Json::parse_error& e) {
			blog.newEntry<BLOG::ERR>("Exception triggered trying to parse JSON file:"
				" \"{}\" [{}]", path.string(), e.what());
		}
	}
	return false;
}

/*==================================================================*/

bool CoreRegistry::validate_by_sha1_hash(const char* fileData, std::size_t fileSize, const std::string& fileSHA1) noexcept {
	/* placeholder logic */
	[[maybe_unused]] const auto& _1 = fileData;
	[[maybe_unused]] const auto& _2 = fileSize;
	[[maybe_unused]] const auto& _3 = fileSHA1;
	return false;
}

bool CoreRegistry::validate_by_extension(const char* fileData, std::size_t fileSize, const std::string& fileType) noexcept {
	auto it = get_registry_map().find(fileType);
	const auto matchingCores = it != get_registry_map().end() ? &it->second : nullptr;

	if (!matchingCores || matchingCores->empty()) {
		blog.newEntry<BLOG::WRN>(
			"Unable to match Program to an existing System variant!");
		return false;
	} else {
		s_potential_cores.clear();
		for (const auto& core : *matchingCores) {
			if (core.test_game_file(fileData, fileSize)) {
				s_potential_cores.push_back(core);
			}
		}

		if (s_potential_cores.empty()) {
			blog.newEntry<BLOG::WRN>(
				"Program rejected by all eligible System variants!");
			return false;
		} else {
			return true;
		}
	}
}

bool CoreRegistry::validate_game_file(
	const char* fileData, std::size_t fileSize,
	const std::string& fileType,
	const std::string& fileSHA1
) noexcept {
	if (fileSHA1.empty()) {
		return validate_by_extension(fileData, fileSize, fileType);
	} else {
		return validate_by_extension(fileData, fileSize, fileType); // placeholder
		//return validateProgramByHash(fileData, fileSize, fileSHA1);
	}
}

bool CoreRegistry::register_new_core(CoreConstructor&& ctor,
	GameFileTester&& tester, KnownExtensions exts) noexcept
{
	CoreDetails reg = { ctor, tester, std::move(exts), };
	for (const auto& ext : reg.known_extensions) {
		try { get_registry_map()[ext].push_back(reg); }
		catch (const std::exception& e) {
			blog.newEntry<BLOG::ERR>(
				"Exception triggered trying to register Emulator Core! [{}]", e.what());
			return false;
		}
	}
	return true;
}

SystemInterface* CoreRegistry::construct_new_core(std::size_t idx) noexcept {
	try {
		// this will later need to handle choosing a specific core out of all available
		// rather than the first one present, adding flexibility
		s_selected_core = s_potential_cores[idx];
		return s_selected_core.construct_core();
	} catch (const std::exception& e) {
		blog.newEntry<BLOG::ERR>(
			"Exception triggered trying to construct Emulator Core! [{}]", e.what());
		return nullptr;
	}
}

void CoreRegistry::load_game_database(const Path& dbPath) noexcept {
	static const auto defaultPath = ::getBasePath() / Path("programDB.json");
	const auto& checkPath = dbPath.empty() ? defaultPath : dbPath;

	if (!loadJsonFromFile(checkPath, s_game_database)) {
		s_game_database.clear();
		blog.newEntry<BLOG::WRN>("Failed to load ProgramDB:"
			" \"{}\"", checkPath.string());
	} else {
		blog.newEntry<BLOG::INF>("Successfully loaded ProgramDB:"
			" \"{}\"", checkPath.string());
	}
}
