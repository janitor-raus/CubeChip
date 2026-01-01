/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "Well512.hpp"

#include <chrono>

/*==================================================================*/

Well512::Well512() noexcept {
	using chrono = std::chrono::steady_clock;
	const auto seed = chrono::now().time_since_epoch().count();

	for (auto i = 0; i < 16; ++i) {
		m_state[i] = result_type(seed >> i * 2);
	}
}
