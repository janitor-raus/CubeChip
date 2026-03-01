/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "FileImage.hpp"

/*==================================================================*/

/**
 * @brief Small staging struct meant to hold static data that can be used
 *        by system cores to initialize internally, without propagating complex
 *        structures through the inheritance constructor chain.
 */
struct SystemStaging {
	static inline FileImage   file_image{};
	static inline std::string sha1_hash{};

	static void clear() noexcept {
		file_image.clear();
		sha1_hash.clear();
	}
};
