/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "GAMEBOY_CLASSIC.hpp"
#if defined(ENABLE_GAMEBOY_SYSTEM) && defined(ENABLE_GAMEBOY_CLASSIC)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(GAMEBOY_CLASSIC, ".gb")

/*==================================================================*/

GAMEBOY_CLASSIC::GAMEBOY_CLASSIC() {

	setBaseSystemFramerate(cRefreshRate);
	setViewportSizes(true, cScreenSizeX, cScreenSizeY, cResSizeMult, 2);
}

/*==================================================================*/

void GAMEBOY_CLASSIC::instructionLoop() noexcept {
	const auto maxCycles{ static_cast<s32>(cCylesPerSec / cRefreshRate) };

	auto curCycles{ 0 };
	while (curCycles < maxCycles) {

	}
}

void GAMEBOY_CLASSIC::renderAudioData() {

}

void GAMEBOY_CLASSIC::renderVideoData() {

}

#endif
