/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string_view>

/*==================================================================*/

// SDL-backed pref-path getter. The first call is statically cached and reused thereafter.
std::string_view get_home_path(
	const char* org = nullptr,
	const char* app = nullptr
) noexcept;

// SDL-backed base-path getter. The first call is statically cached and reused thereafter.
std::string_view get_base_path() noexcept;
