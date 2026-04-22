/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

//#define ENABLE_GAMEBOY_SYSTEM
#ifdef ENABLE_GAMEBOY_SYSTEM

#include "../ISystem.hpp"

/*==================================================================*/

class IFamily_GAMEBOY : public ISystem {

protected:
	enum STREAM { MAIN };
	enum VOICE {
		ID_0, ID_1, ID_2, ID_3, COUNT
	};

	static inline Path s_savestate_path{};

	AudioDevice m_audio_device;

	std::vector<SimpleKeyMapping> m_custom_binds;

	u32  getKeyStates();
	void load_preset_binds();

	template <IsContiguousContainer T> requires
		SameValueTypes<T, decltype(m_custom_binds)>
	void load_custom_binds(const T& binds) {
		m_custom_binds.assign(std::begin(binds), std::end(binds));
	}

	void copy_game_to_memory(u8* dest) noexcept;

	virtual void update_key_states() noexcept = 0;
	virtual void instruction_loop() noexcept = 0;
	virtual void push_audio_data() = 0;
	virtual void push_video_data() = 0;

protected:
	IFamily_GAMEBOY() noexcept;

public:
	void main_system_loop() override final;
};

#endif
