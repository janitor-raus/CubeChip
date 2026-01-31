/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BasicInput.hpp"
#include "HomeDirManager.hpp"

#include "GameBoy_CoreInterface.hpp"

#ifdef ENABLE_GAMEBOY_SYSTEM

/*==================================================================*/

GameBoy_CoreInterface::GameBoy_CoreInterface() noexcept {
	if (const auto* path{ HDM->addSystemDir("savestate", "GAMEBOY") })
		{ s_savestate_path = *path / HDM->getFileSHA1(); }

	m_audio_device.addAudioStream(STREAM::MAIN, 48'000, 1);
	m_audio_device.resumeStreams();

	load_preset_binds();
}

/*==================================================================*/

void GameBoy_CoreInterface::main_system_loop() {
	if (!isSystemRunning())
		[[unlikely]] { return; }

	update_key_states();
	instruction_loop();
	push_audio_data();
	push_video_data();
	pushOverlayData();
}

void GameBoy_CoreInterface::load_preset_binds() {
	static constexpr auto _{ SDL_SCANCODE_UNKNOWN };
	static constexpr SimpleKeyMapping defaultKeyMappings[]{
		{0x7, KEY(G), _}, // START
		{0x6, KEY(F), _}, // SELECT
		{0x5, KEY(Q), _}, // B
		{0x4, KEY(E), _}, // A
		{0x3, KEY(S), _}, // ↓
		{0x2, KEY(W), _}, // ↑
		{0x1, KEY(A), _}, // ←
		{0x0, KEY(D), _}, // →
	};

	load_custom_binds(std::span(defaultKeyMappings));
}

u32  GameBoy_CoreInterface::getKeyStates() {
	auto keyStates{ 0u };

	Input->updateStates();

	for (const auto& mapping : m_custom_binds) {
		if (Input->areAnyHeld(mapping.key, mapping.alt)) {
			keyStates |= 1u << mapping.idx;
		}
	}

	return keyStates;
}

void GameBoy_CoreInterface::copy_game_to_memory(u8* dest) noexcept {
	std::copy_n(EXEC_POLICY(unseq)
		HDM->getFileData(),
		HDM->getFileSize(),
		dest
	);
}

#endif
