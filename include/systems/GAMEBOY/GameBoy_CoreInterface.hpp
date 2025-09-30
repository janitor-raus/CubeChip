/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

//#define ENABLE_GAMEBOY_SYSTEM
#ifdef ENABLE_GAMEBOY_SYSTEM

#include "../SystemInterface.hpp"

/*==================================================================*/

class GameBoy_CoreInterface : public SystemInterface {

protected:
	enum STREAM { MAIN };
	enum VOICE {
		ID_0, ID_1, ID_2, ID_3, COUNT
	};

	static inline Path sSavestatePath{};

	AudioDevice mAudioDevice;

	std::vector<SimpleKeyMapping> mCustomBinds;

	u32  getKeyStates();
	void loadPresetBinds();

	template <IsContiguousContainer T> requires
		SameValueTypes<T, decltype(mCustomBinds)>
	void loadCustomBinds(const T& binds) {
		mCustomBinds.assign(std::begin(binds), std::end(binds));
	}

	void copyGameToMemory(u8* dest) noexcept;

	virtual void updateKeyStates() noexcept = 0;
	virtual void instructionLoop() noexcept = 0;
	virtual void renderAudioData() = 0;
	virtual void renderVideoData() = 0;

protected:
	GameBoy_CoreInterface() noexcept;

public:
	void mainSystemLoop() override;
};

#endif
