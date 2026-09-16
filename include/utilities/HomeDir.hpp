/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string_view>

/*==================================================================*/

namespace HomeDir {
	std::string_view path() noexcept;

	// Can only be called once per run. The path will be empty
	// if initialization fails or is not called.
	void init(
		std::string_view home_override, bool portable,
		std::string_view org, std::string_view app
	) noexcept;
};
