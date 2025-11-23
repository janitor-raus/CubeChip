/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_filesystem.h>

#include "PathGetters.hpp"

/*==================================================================*/

const char* getHomePath(
	const char* org,
	const char* app
) noexcept {
	static auto* homePath = SDL_GetPrefPath(org, app);
	return homePath;
}

const char* getBasePath() noexcept {
	static auto* basePath = SDL_GetBasePath();
	return basePath;
}
