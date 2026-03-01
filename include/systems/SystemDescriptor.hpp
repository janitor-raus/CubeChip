/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string_view>
#include <span>

/*==================================================================*/

/**
 * @brief Small descriptor struct meant to assist the CoreRegistry in evaluating
 *        Core registrations and program suitability when loading a file.
 */
struct SystemDescriptor {
	using FileTestCallable = const char* (*)(std::span<const char>) noexcept;
	using KnownExtensions = std::span<const std::string_view>;

	unsigned long long core_version{}; // general purpose version number

	std::string_view family_pretty_name{}; // pretty name of the system family, used for display purposes
	std::string_view family_name{}; // short name of the system family, used for grouping purposes
	std::string_view family_desc{}; // longer description of the system family, used for display

	std::string_view system_pretty_name{}; // pretty name of the system, used for display purposes
	std::string_view system_name{}; // short name of the system, used for display and file association
	std::string_view system_desc{}; // longer description of the system, used for display

	KnownExtensions  known_extensions{}; // file extensions usually associated with the system
	FileTestCallable validate_program{}; // simple callable to statically check a game's compatibility with the system
};
