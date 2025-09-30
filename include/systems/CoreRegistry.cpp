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

Json CoreRegistry::sProgramDB{};
Json CoreRegistry::sCoreConfig{};

/*==================================================================*/

bool CoreRegistry::validateProgramByHash(const char* fileData, size_type fileSize, const Str& fileSHA1) noexcept {
	/* placeholder logic */
	[[maybe_unused]] const auto& _1{ fileData };
	[[maybe_unused]] const auto& _2{ fileSize };
	[[maybe_unused]] const auto& _3{ fileSHA1 };
	return false;
}

bool CoreRegistry::validateProgramByType(const char* fileData, size_type fileSize, const Str& fileType) noexcept {
	const auto matchingCores{ findEligibleCores(fileType) };

	if (!matchingCores || matchingCores->empty()) {
		blog.newEntry<BLOG::WARN>(
			"Unable to match Program to an existing System variant!");
		return false;
	} else {
		sEligible.clear();
		for (const auto& core : *matchingCores) {
			if (core.testProgram(fileData, fileSize)) {
				sEligible.push_back(core);
			}
		}

		if (sEligible.empty()) {
			blog.newEntry<BLOG::WARN>(
				"Program rejected by all eligible System variants!");
			return false;
		} else {
			return true;
		}
	}
}

bool CoreRegistry::validateProgram(const char* fileData, size_type fileSize, const Str& fileType, const Str& fileSHA1) noexcept {
	if (fileSHA1.empty()) {
		return validateProgramByType(fileData, fileSize, fileType);
	} else {
		return validateProgramByType(fileData, fileSize, fileType); // placeholder
		//return validateProgramByHash(fileData, fileSize, fileSHA1);
	}
}

bool CoreRegistry::registerCore(CoreConstructor&& ctor, ProgramTester&& tester, FileExtList exts) noexcept {
	CoreDetails reg{ ctor, tester, std::move(exts), };
	for (const auto& ext : reg.fileExtensions) {
		try { getRegistry()[ext].push_back(reg); }
		catch (const std::exception& e) {
			blog.newEntry<BLOG::ERROR>(
				"Exception triggered trying to register Emulator Core! [{}]", e.what());
			return false;
		}
	}
	return true;
}

const CoreRegList* CoreRegistry::findEligibleCores(const Str& ext) noexcept {
	auto it{ getRegistry().find(ext) };
	return it != getRegistry().end() ? &it->second : nullptr;
}

SystemInterface* CoreRegistry::constructCore(size_type idx) noexcept {
	try {
		// this will later need to handle choosing a specific core out of all available
		// rather than the first one present, adding flexibility
		sCurrentCore = sEligible[idx];
		return sCurrentCore.constructCore();
	} catch (const std::exception& e) {
		blog.newEntry<BLOG::ERROR>(
			"Exception triggered trying to construct Emulator Core! [{}]", e.what());
		return nullptr;
	}
}

void CoreRegistry::loadProgramDB(const Path& dbPath) noexcept {
	static const auto defaultPath{ ::getBasePath() / Path("programDB.json") };
	const auto& checkPath{ dbPath.empty() ? defaultPath : dbPath };

	if (!loadJsonFromFile(checkPath, sProgramDB)) {
		sProgramDB.clear();
		blog.newEntry<BLOG::WARN>(
			"Failed to load ProgramDB: \"{}\"", checkPath.string());
	} else {
		blog.newEntry<BLOG::INFO>(
			"Successfully loaded ProgramDB: \"{}\"", checkPath.string());
	}
}

bool CoreRegistry::loadJsonFromFile(const Path& path, Json& output) noexcept {
	if (auto jsonData = ::readFileData(path)) {
		try {
			output = Json::parse(jsonData->begin(), jsonData->end());
			return true;
		} catch (const Json::parse_error& e) {
			blog.newEntry<BLOG::ERROR>(
				"Exception triggered trying to parse JSON file: \"{}\" [{}]", path.string(), e.what());
		}
	}
	return false;
}
