/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../Chip8_CoreInterface.hpp"

#define ENABLE_SCHIP_LEGACY
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_SCHIP_LEGACY)

/*==================================================================*/

class SCHIP_LEGACY final : public Chip8_CoreInterface {
	static constexpr u64 cTotalMemory{ KiB(4) };
	static constexpr u32 cSafezoneOOB{    32 };
	static constexpr u32 cGameLoadPos{   512 };
	static constexpr u32 cStartOffset{   512 };
	static constexpr f32 cRefreshRate{ 64.0f };

	static constexpr s32 cResSizeMult{   4 };
	static constexpr s32 cScreenSizeX{ 128 };
	static constexpr s32 cScreenSizeY{  64 };
	static constexpr s32 cInstSpeedHi{  45 };
	static constexpr s32 cInstSpeedLo{  32 };

/*==================================================================*/

	FixedMap2D<u8, cScreenSizeX, cScreenSizeY>
		mDisplayBuffer;

	std::array<u8, cTotalMemory + cSafezoneOOB>
		mMemoryBank{};

	template <std::integral T>
	void writeMemoryI(T value, u32 pos) noexcept {
		const auto index{ mRegisterI + pos };
		const auto valid{ index < cTotalMemory ? index : cTotalMemory + cSafezoneOOB - 1 };
		::assign_cast(mMemoryBank[valid], value);
	}

	auto readMemoryI(u32 pos) const noexcept {
		return mMemoryBank[mRegisterI + pos];
	}

/*==================================================================*/

public:
	SCHIP_LEGACY() {}

	static constexpr bool validateProgram(
		const char* fileData,
		const size_type fileSize
	) noexcept {
		if (!fileData || !fileSize) { return false; }
		return fileSize + cGameLoadPos <= cTotalMemory;
	}

	s32 getMaxDisplayW() const noexcept override { return cScreenSizeX; }
	s32 getMaxDisplayH() const noexcept override { return cScreenSizeY; }

private:
	void initializeSystem() noexcept override;
	void handleCycleLoop() noexcept override;

	template <typename Lambda>
	void instructionLoop(Lambda&& condition) noexcept;

	void renderAudioData() override;
	void renderVideoData() override;

	void prepDisplayArea(const Resolution mode) override;

	void scrollDisplayDN(s32 N);
	void scrollDisplayLT();
	void scrollDisplayRT();

/*==================================================================*/
	#pragma region 0 instruction branch

	// 00DN - scroll plane N lines down
	void instruction_00CN(s32 N) noexcept;
	// 00E0 - erase whole display
	void instruction_00E0() noexcept;
	// 00EE - return from subroutine
	void instruction_00EE() noexcept;
	// 00FB - scroll plane 4 pixels right
	void instruction_00FB() noexcept;
	// 00FC - scroll plane 4 pixels left
	void instruction_00FC() noexcept;
	// 00FD - stop signal
	void instruction_00FD() noexcept;
	// 00FE - display res == 64x32
	void instruction_00FE() noexcept;
	// 00FF - display res == 128x64
	void instruction_00FF() noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	// 1NNN - jump to NNN
	void instruction_1NNN(s32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	// 2NNN - call subroutine at NNN
	void instruction_2NNN(s32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	// 3XNN - skip next instruction if VX == NN
	void instruction_3xNN(s32 X, s32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	// 4XNN - skip next instruction if VX != NN
	void instruction_4xNN(s32 X, s32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	// 5XY0 - skip next instruction if VX == VY
	void instruction_5xy0(s32 X, s32 Y) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	// 6XNN - set VX = NN
	void instruction_6xNN(s32 X, s32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	// 7XNN - set VX = VX + NN
	void instruction_7xNN(s32 X, s32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	// 8XY0 - set VX = VY
	void instruction_8xy0(s32 X, s32 Y) noexcept;
	// 8XY1 - set VX = VX | VY
	void instruction_8xy1(s32 X, s32 Y) noexcept;
	// 8XY2 - set VX = VX & VY
	void instruction_8xy2(s32 X, s32 Y) noexcept;
	// 8XY3 - set VX = VX ^ VY
	void instruction_8xy3(s32 X, s32 Y) noexcept;
	// 8XY4 - set VX = VX + VY, VF = carry
	void instruction_8xy4(s32 X, s32 Y) noexcept;
	// 8XY5 - set VX = VX - VY, VF = !borrow
	void instruction_8xy5(s32 X, s32 Y) noexcept;
	// 8XY7 - set VX = VY - VX, VF = !borrow
	void instruction_8xy7(s32 X, s32 Y) noexcept;
	// 8XY6 - set VX = VX >> 1, VF = carry
	void instruction_8xy6(s32 X, s32  ) noexcept;
	// 8XYE - set VX = VX << 1, VF = carry
	void instruction_8xyE(s32 X, s32  ) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	// 9XY0 - skip next instruction if VX != VY
	void instruction_9xy0(s32 X, s32 Y) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	// ANNN - set I = NNN
	void instruction_ANNN(s32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	// BXNN - jump to NNN + VX
	void instruction_BXNN(s32 X, s32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	// CXNN - set VX = rnd(256) & NN
	void instruction_CxNN(s32 X, s32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	bool drawSingleBytes(s32 X, s32 Y, s32 WIDTH, s32 DATA) noexcept;
	bool drawDoubleBytes(s32 X, s32 Y, s32 WIDTH, s32 DATA) noexcept;

	// DXYN - draw N sprite rows at VX and VY
	void instruction_DxyN(s32 X, s32 Y, s32 N) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	// EX9E - skip next instruction if key VX down (p1)
	void instruction_Ex9E(s32 X) noexcept;
	// EXA1 - skip next instruction if key VX up (p1)
	void instruction_ExA1(s32 X) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	// FX07 - set VX = delay timer
	void instruction_Fx07(s32 X) noexcept;
	// FX0A - set VX = key, wait for keypress
	void instruction_Fx0A(s32 X) noexcept;
	// FX15 - set delay timer = VX
	void instruction_Fx15(s32 X) noexcept;
	// FX18 - set sound timer = VX
	void instruction_Fx18(s32 X) noexcept;
	// FX1E - set I = I + VX
	void instruction_Fx1E(s32 X) noexcept;
	// FX29 - set I to 5-byte hex sprite from VX
	void instruction_Fx29(s32 X) noexcept;
	// FX30 - set I to 10-byte hex sprite from VX
	void instruction_Fx30(s32 X) noexcept;
	// FX33 - store BCD of VX to RAM at I..I+2
	void instruction_Fx33(s32 X) noexcept;
	// FN55 - store V0..VN to RAM at I..I+N
	void instruction_FN55(s32 N) noexcept;
	// FN65 - load V0..VN from RAM at I..I+N
	void instruction_FN65(s32 N) noexcept;
	// FN75 - store V0..VN to the permanent regs
	void instruction_FN75(s32 N) noexcept;
	// FN85 - load V0..VN from the permanent regs
	void instruction_FN85(s32 N) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
};

#endif
