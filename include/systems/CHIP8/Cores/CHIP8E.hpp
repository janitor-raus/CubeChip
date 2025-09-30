/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../Chip8_CoreInterface.hpp"

#define ENABLE_CHIP8E
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_CHIP8E)

/*==================================================================*/

class CHIP8E final : public Chip8_CoreInterface {
	static constexpr u64 cTotalMemory{ KiB(16) };
	static constexpr u32 cSafezoneOOB{    32 };
	static constexpr u32 cGameLoadPos{   512 };
	static constexpr u32 cStartOffset{   512 };
	static constexpr f32 cRefreshRate{ 60.0f };

	static constexpr s32 cResSizeMult{  8 };
	static constexpr s32 cScreenSizeX{ 64 };
	static constexpr s32 cScreenSizeY{ 32 };
	static constexpr s32 cInstSpeedHi{ 30 };
	static constexpr s32 cInstSpeedLo{ 15 };

	static constexpr u32 cMaxDisplayW{ 64 };
	static constexpr u32 cMaxDisplayH{ 32 };

private:
	std::array<u8, cScreenSizeX * cScreenSizeY>
		mDisplayBuffer{};

	std::array<u8, cTotalMemory + cSafezoneOOB>
		mMemoryBank{};

	template <std::integral T>
	void writeMemoryI(T value, u32 pos) noexcept {
		const auto index{ mRegisterI + pos };
		const auto valid{ index < cTotalMemory ? index : cTotalMemory + cSafezoneOOB - 1 };
		::assign_cast(mMemoryBank[valid], value);
	}

	template <std::integral T>
	void writeMemory(T value, u32 pos) noexcept {
		const auto valid{ pos < cTotalMemory ? pos : cTotalMemory + cSafezoneOOB - 1 };
		::assign_cast(mMemoryBank[valid], value);
	}

	auto readMemoryI(u32 pos) const noexcept {
		return mMemoryBank[mRegisterI + pos];
	}

public:
	CHIP8E() {}

	static constexpr bool validateProgram(
		const char* fileData,
		const size_type fileSize
	) noexcept {
		if (!fileData || !fileSize) { return false; }
		return fileSize + cGameLoadPos <= cTotalMemory;
	}

	s32 getMaxDisplayW() const noexcept override { return cMaxDisplayW; }
	s32 getMaxDisplayH() const noexcept override { return cMaxDisplayH; }

private:
	void initializeSystem() noexcept override;
	void handleCycleLoop() noexcept override;

	template <typename Lambda>
	void instructionLoop(Lambda&& condition) noexcept;

	void renderAudioData() override;
	void renderVideoData() override;

	void prepDisplayArea(const Resolution) override {}

/*==================================================================*/
	#pragma region 0 instruction branch

	// 00E0 - erase whole display
	void instruction_00E0() noexcept;
	// 00EE - return from subroutine
	void instruction_00EE() noexcept;
	// 00ED - stop signal
	void instruction_00ED() noexcept;
	// 00F2 - no operation
	void instruction_00F2() noexcept;
	// 0151 - wait for delay timer == 0
	void instruction_0151() noexcept;
	// 0188 - skip next instruction
	void instruction_0188() noexcept;

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
	// 5XY1 - skip next instruction if VX > VY
	void instruction_5xy1(s32 X, s32 Y) noexcept;
	// 5XY2 - store range of registers to memory
	void instruction_5xy2(s32 X, s32 Y) noexcept;
	// 5XY3 - load range of registers from memory
	void instruction_5xy3(s32 X, s32 Y) noexcept;

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
	// 8XY6 - set VX = VY >> 1, VF = carry
	void instruction_8xy6(s32 X, s32 Y) noexcept;
	// 8XYE - set VX = VY << 1, VF = carry
	void instruction_8xyE(s32 X, s32 Y) noexcept;

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

	void instruction_BBNN(s32 NN) noexcept;
	void instruction_BFNN(s32 NN) noexcept;

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

	void drawByte(s32 X, s32 Y, u32 DATA) noexcept;

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

	// Fx03 - write content of VX to output port 3
	void instruction_Fx03(s32 X) noexcept;
	// FX07 - set VX = delay timer
	void instruction_Fx07(s32 X) noexcept;
	// FX0A - set VX = key, wait for keypress
	void instruction_Fx0A(s32 X) noexcept;
	// FX15 - set delay timer = VX
	void instruction_Fx15(s32 X) noexcept;
	// FX18 - set sound timer = VX
	void instruction_Fx18(s32 X) noexcept;
	// Fx1B - skip VX amount of bytes
	void instruction_Fx1B(s32 X) noexcept;
	// FX1E - set I = I + VX
	void instruction_Fx1E(s32 X) noexcept;
	// FX29 - set I to 5-byte hex sprite from VX
	void instruction_Fx29(s32 X) noexcept;
	// FX33 - store BCD of VX to RAM at I..I+2
	void instruction_Fx33(s32 X) noexcept;
	// FX4F - set delay timer to VX and wait
	void instruction_Fx4F(s32 X) noexcept;
	// FN55 - store V0..VN to RAM at I..I+N
	void instruction_FN55(s32 N) noexcept;
	// FN65 - load V0..VN from RAM at I..I+N
	void instruction_FN65(s32 N) noexcept;
	// FxE3 - wait for strobe at ^EF4, read content of input port 3 to VX
	void instruction_FxE3(s32 X) noexcept;
	// FxE7 - read content of input port 3 to VX without waiting for strobe
	void instruction_FxE7(s32 X) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
};

#endif
