/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_filesystem.h>

#include "PathGetters.hpp"

/*==================================================================*/

std::string_view get_home_path(
	const char* org,
	const char* app
) noexcept {
	static std::string_view home_path = [&]() noexcept {
		auto* path = SDL_GetPrefPath(org, app);
		return path ? path : "";
	}();
	return home_path;
}

std::string_view get_base_path() noexcept {
	static std::string_view base_path = [&]() noexcept {
		auto* path = SDL_GetBasePath();
		return path ? path : "";
	}();
	return base_path;
}
