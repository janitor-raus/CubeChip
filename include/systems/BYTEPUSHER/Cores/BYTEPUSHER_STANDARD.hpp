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
	static constexpr u64 cTotalMemory{ MiB(16) };
	static constexpr u32 cSafezoneOOB{     8 };
	static constexpr f32 cRefreshRate{ 60.0f };

	static constexpr s32 cAudioLength{ 256 };
	static constexpr s32 cResSizeMult{   2 };
	static constexpr s32 cScreenSizeX{ 256 };
	static constexpr s32 cScreenSizeY{ 256 };

	static constexpr u32 cMaxDisplayW{ 256 };
	static constexpr u32 cMaxDisplayH{ 256 };

private:
	std::array<u8, cTotalMemory + cSafezoneOOB>
		mMemoryBank{};

	template<u32 T> requires (T >= 1 && T <= 3)
		u32 readData(u32 pos) const noexcept {
		if        constexpr (T == 1) {
			return mMemoryBank[pos + 0];
		} else if constexpr (T == 2) {
			return mMemoryBank[pos + 0] << 8
				 | mMemoryBank[pos + 1];
		} else if constexpr (T == 3) {
			return mMemoryBank[pos + 0] << 16
				 | mMemoryBank[pos + 1] << 8
				 | mMemoryBank[pos + 2];
		}
	}

	void instructionLoop() noexcept override;
	void renderAudioData() override;
	void renderVideoData() override;

public:
	BYTEPUSHER_STANDARD();

	static constexpr bool validateProgram(
		const char* fileData,
		const size_type fileSize
	) noexcept {
		if (!fileData || !fileSize) { return false; }
		return fileSize <= cTotalMemory;
	}

	s32 getMaxDisplayW() const noexcept override { return cScreenSizeX; }
	s32 getMaxDisplayH() const noexcept override { return cScreenSizeY; }
};

#endif
