/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../BytePusher_CoreInterface.hpp"

#define ENABLE_BYTEPUSHER_STANDARD
#if defined(ENABLE_BYTEPUSHER_SYSTEM) && defined(ENABLE_BYTEPUSHER_STANDARD)

/*==================================================================*/

class BYTEPUSHER_STANDARD final : public BytePusher_CoreInterface {
	static constexpr u64 c_sys_memory_size = 16_MiB;
	static constexpr u32 cSafezoneOOB =     8;
	static constexpr f32 c_sys_refresh_rate = 60.0f;

	static constexpr s32 cAudioLength = 256;
	static constexpr s32 cResSizeMult =   2;
	static constexpr s32 c_sys_screen_W = 256;
	static constexpr s32 c_sys_screen_H = 256;

	static constexpr u32 cMaxDisplayW = 256;
	static constexpr u32 cMaxDisplayH = 256;

private:
	std::array<u8, c_sys_memory_size + cSafezoneOOB>
		m_memory_bank{};

	template<u32 T> requires (T >= 1 && T <= 3)
		u32 readData(u32 pos) const noexcept {
		if        constexpr (T == 1) {
			return m_memory_bank[pos + 0];
		} else if constexpr (T == 2) {
			return m_memory_bank[pos + 0] << 8
				 | m_memory_bank[pos + 1];
		} else if constexpr (T == 3) {
			return m_memory_bank[pos + 0] << 16
				 | m_memory_bank[pos + 1] << 8
				 | m_memory_bank[pos + 2];
		}
	}

	void instruction_loop() noexcept override;
	void push_audio_data() override;
	void push_video_data() override;

public:
	BYTEPUSHER_STANDARD() noexcept
		: BytePusher_CoreInterface(DisplayDevice(c_sys_screen_W, c_sys_screen_H, "BytePusher Standard"))
	{}

	static constexpr bool validate_program(
		const char* fileData,
		const size_type fileSize
	) noexcept {
		if (!fileData || !fileSize) { return false; }
		return fileSize <= c_sys_memory_size;
	}

private:
	void initialize_system() noexcept override;
};

#endif
