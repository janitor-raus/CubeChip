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

static bool load_json_from_file(std::string_view json_file_path, Json& output) noexcept {
	if (auto json_data = ::read_file_data(json_file_path)) {
		try {
			output = Json::parse(json_data->begin(), json_data->end());
			return true;
		} catch (const Json::parse_error& e) {
			blog.error("Exception triggered trying to parse JSON file:"
				" \"{}\" [{}]", json_file_path, e.what());
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

bool CoreRegistry::validate_by_extension(const char* file_data, std::size_t file_size, const std::string& file_type) noexcept {
	auto it = get_registry_map().find(file_type);
	const auto matched_cores = it != get_registry_map().end() ? &it->second : nullptr;

	if (!matched_cores || matched_cores->empty()) {
		blog.warn("Unable to match Program to an existing System variant!");
		return false;
	} else {
		s_potential_cores.clear();
		for (const auto& core : *matched_cores) {
			if (core.test_game_file(file_data, file_size)) {
				s_potential_cores.push_back(core);
			}
		}

		if (s_potential_cores.empty()) {
			blog.warn("Program rejected by all eligible System variants!");
			return false;
		} else {
			return true;
		}
	}
}

bool CoreRegistry::validate_game_file(
	const char* file_data, std::size_t file_size,
	const std::string& file_type,
	const std::string& file_sha1
) noexcept {
	if (file_sha1.empty()) {
		return validate_by_extension(file_data, file_size, file_type);
	} else {
		return validate_by_extension(file_data, file_size, file_type); // placeholder
		//return validateProgramByHash(file_data, file_size, file_sha1);
	}
}

bool CoreRegistry::register_new_core(CoreConstructor&& ctor,
	GameFileTester&& tester, KnownExtensions exts) noexcept
{
	CoreDetails reg = { ctor, tester, std::move(exts), };
	for (const auto& ext : reg.known_extensions) {
		try { get_registry_map()[ext].push_back(reg); }
		catch (const std::exception& e) {
			blog.error("Exception triggered trying to register Emulator Core! [{}]", e.what());
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
		blog.error("Exception triggered trying to construct Emulator Core! [{}]", e.what());
		return nullptr;
	}
}

void CoreRegistry::load_game_database(std::string_view db_file_path) noexcept {
	static const auto default_db_path = (::get_base_path() / fs::Path("programDB.json")).string();
	std::string_view normalized_path = db_file_path.empty() ? default_db_path : db_file_path;

	if (!load_json_from_file(normalized_path, s_game_database)) {
		s_game_database.clear();
		blog.warn("Failed to load ProgramDB: \"{}\"", normalized_path);
	} else {
		blog.warn("Successfully loaded ProgramDB: \"{}\"", normalized_path);
	}
}
