/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_filesystem.h>

#include "PathGetters.hpp"

/*==================================================================*/

const char* get_home_path(
	const char* org,
	const char* app
) noexcept {
	static auto* home_path = SDL_GetPrefPath(org, app);
	return home_path;
}

const char* get_base_path() noexcept {
	static auto* base_path = SDL_GetBasePath();
	return base_path;
}
