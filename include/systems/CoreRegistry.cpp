/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <vector>
#include <tuple>

#include "nlohmann/json.hpp"
#include "BasicLogger.hpp"
#include "PathGetters.hpp"
#include "SimpleFileIO.hpp"
#include "CoreRegistry.hpp"
#include "SystemDescriptor.hpp"

/*==================================================================*/

using Json = nlohmann::json;

static Json s_game_database;
static Json s_custom_core_cfg;

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

/*==================================================================*/

static auto& get_registry() noexcept {
	static std::vector<CoreRegistry::WeakHook>
		s_registered_systems{};

	return s_registered_systems;
}

auto CoreRegistry::get_available_core_span() noexcept -> std::span<const WeakHook> {
	return get_registry();
}

void CoreRegistry::insert_new_registration(const LiveHook& entry) noexcept {
	get_registry().push_back(entry);
}

/*==================================================================*/

auto CoreRegistry::get_candidate_core_span() noexcept -> std::span<const LiveHook> {
	static std::vector<LiveHook>
		s_candidate_systems{};

	s_candidate_systems.clear();

	for (const auto& entry_ptr : get_registry()) {
		if (const auto entry = entry_ptr.lock()) {
			s_candidate_systems.push_back(entry);
		}
	}

	std::sort(s_candidate_systems.begin(), s_candidate_systems.end(),
		[](const auto& lhs, const auto& rhs) {
			const auto& ldesc = *lhs->descriptor;
			const auto& rdesc = *rhs->descriptor;
			return std::tie(ldesc.family_name, ldesc.system_name)
				 < std::tie(rdesc.family_name, rdesc.system_name);
		});

	return s_candidate_systems;
}
