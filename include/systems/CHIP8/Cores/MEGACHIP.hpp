/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../Chip8_CoreInterface.hpp"

#define ENABLE_MEGACHIP
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_MEGACHIP)

/*==================================================================*/

class MEGACHIP final : public Chip8_CoreInterface {
	static constexpr u64 cTotalMemory{ MiB(16) };
	static constexpr u32 cSafezoneOOB{    32 };
	static constexpr u32 cGameLoadPos{   512 };
	static constexpr u32 cStartOffset{   512 };
	static constexpr f32 cRefreshRate{ 60.0f };

	static constexpr s32 cResSizeMult{   2 };
	static constexpr s32 cScreenSizeX{ 128 };
	static constexpr s32 cScreenSizeY{  64 };
	static constexpr s32 cInstSpeedHi{  45 };
	static constexpr s32 cInstSpeedLo{  30 };

	static constexpr s32 cScreenMegaX{  256 };
	static constexpr s32 cScreenMegaY{  192 };
	static constexpr s32 cInstSpeedMC{ 3000 };

	static constexpr u32 cMaxDisplayW{ 256 };
	static constexpr u32 cMaxDisplayH{ 192 };

/*==================================================================*/

	void initializeFontColors() noexcept;

	struct Texture {
		s32 W{}, H{};
		s32 collide{ 0xFF };
		s32 opacity{ 0xFF };
		u32 fontOffset{};

		constexpr void reset() noexcept
			{ *this = Texture{}; }
	} mTexture;

	enum BlendMode {
		ALPHA_BLEND  = 0,
		LINEAR_DODGE = 4,
		MULTIPLY     = 5,
	};

	RGBA::BlendFunc mBlendFunc;

	void selectBlendingAlgo(s32 mode) noexcept;

	void scrapAllVideoBuffers();
	void flushAllVideoBuffers();
	void blendAndFlushBuffers() const;

	struct TrackData {
		u8*  data{};
		u32  size{};
		bool loop{};

		constexpr void reset() noexcept
			{ *this = TrackData{}; }

		constexpr bool isOn() const noexcept
			{ return data != nullptr; }

		constexpr auto pos(Phase head) const noexcept
			{ return data[u32(head * size)] - 128; }
	} mTrack;

	void startAudioTrack(bool repeat) noexcept;

	static void makeByteWave(f32* data, u32 size, Voice* voice, Stream*) noexcept;

	FixedMap2D<u8, cScreenSizeX, cScreenSizeY>
		mDisplayBuffer; // legacy 128x64 buffer

	FixedMap2D<RGBA, cScreenMegaX, cScreenMegaY>
		mLastRenderBuffer; // buffer of last rendered frame
	FixedMap2D<RGBA, cScreenMegaX, cScreenMegaY>
		mBackgroundBuffer; // primary draw buffer
	FixedMap2D<u8, cScreenMegaX, cScreenMegaY>
		mCollisionMap; // collision detection map based on palette index
	FixedMap2D<RGBA, 256, 1>
		mColorPalette; // 256-color palette

	std::array<RGBA, 10> mFontColor{};

	std::array<u8, cTotalMemory + cSafezoneOOB>
		mMemoryBank{};

	template <std::integral T>
	void writeMemoryI(T value, u32 pos) noexcept {
		const auto index{ mRegisterI + pos };
		const auto valid{ index < cTotalMemory ? index : cTotalMemory + cSafezoneOOB - 1 };
		::assign_cast(mMemoryBank[valid], value);
	}

	auto readMemory(u32 pos) const noexcept {
		return mMemoryBank[pos];
	}

	auto readMemoryI(u32 pos) const noexcept {
		return mMemoryBank[mRegisterI + pos];
	}

	u32 incIndexRegister(u32 value) noexcept override final
		{ return mRegisterI = (mRegisterI + value) & 0xFFFFFF; }

	u32 decIndexRegister(u32 value) noexcept override final
		{ return mRegisterI = (mRegisterI - value) & 0xFFFFFF; }

	u32 setIndexRegister(u32 value) noexcept override final
		{ return mRegisterI = value & 0xFFFFFF; }

/*==================================================================*/

	auto NNNN() const noexcept { return mMemoryBank[mCurrentPC] << 8 | mMemoryBank[mCurrentPC + 1]; }

public:
	MEGACHIP() {}

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

	void prepDisplayArea(const Resolution mode) override;

	void skipInstruction() noexcept override;

	void scrollDisplayUP(s32 N);
	void scrollDisplayDN(s32 N);
	void scrollDisplayLT();
	void scrollDisplayRT();

	void scrollBuffersUP(s32 N);
	void scrollBuffersDN(s32 N);
	void scrollBuffersLT();
	void scrollBuffersRT();

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

	// 00DN - scroll plane N lines up
	void instruction_00BN(s32 N) noexcept;
	// 0010 - disable mega mode
	void instruction_0010() noexcept;
	// 0011 - enable mega mode
	void instruction_0011() noexcept;
	// 01NN - set I to NN'NNNN
	void instruction_01NN(s32 NN) noexcept;
	// 02NN - load NN palette colors from RAM at I
	void instruction_02NN(s32 NN) noexcept;
	// 03NN - set sprite width to NN
	void instruction_03NN(s32 NN) noexcept;
	// 04NN - set sprite height to NN
	void instruction_04NN(s32 NN) noexcept;
	// 05NN - set screen brightness to NN
	void instruction_05NN(s32 NN) noexcept;
	// 060N - start digital sound from RAM at I, repeat if N == 0
	void instruction_060N(s32 N) noexcept;
	// 0700 - stop digital sound
	void instruction_0700() noexcept;
	// 080N - set blend mode to N
	void instruction_080N(s32 N) noexcept;
	// 09NN - set collision color to palette entry NN
	void instruction_09NN(s32 NN) noexcept;

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
