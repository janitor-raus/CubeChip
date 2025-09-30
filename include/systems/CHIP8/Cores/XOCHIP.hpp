/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../Chip8_CoreInterface.hpp"

#define ENABLE_XOCHIP
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_XOCHIP)

/*==================================================================*/

class XOCHIP final : public Chip8_CoreInterface {
	static constexpr u64 cTotalMemory{ KiB(64) };
	static constexpr u32 cSafezoneOOB{   128 };
	static constexpr u32 cGameLoadPos{   512 };
	static constexpr u32 cStartOffset{   512 };
	static constexpr f32 cRefreshRate{ 60.0f };

	static constexpr s32 cResSizeMult{     8 };
	static constexpr s32 cScreenSizeX{    64 };
	static constexpr s32 cScreenSizeY{    32 };
	static constexpr s32 cInstSpeedHi{ 50000 };
	static constexpr s32 cInstSpeedLo{  1000 };

	static constexpr u32 cMaxDisplayW{ 128 };
	static constexpr u32 cMaxDisplayH{  64 };

private:
	std::array<RGBA, 16> mBitColors{};

	// 332 RGB color mapping: SHR 5 | SHR 2 | SHR 0
	// R/G: 0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xFF
	//  B : 0x00,             0x60,       0xA0,       0xFF
	static constexpr std::array<RGBA, 256> sColorPalette{ {
		0x00000000, 0x00006000, 0x0000A000, 0x0000FF00,
		0x00200000, 0x00206000, 0x0020A000, 0x0020FF00,
		0x00400000, 0x00406000, 0x0040A000, 0x0040FF00,
		0x00600000, 0x00606000, 0x0060A000, 0x0060FF00,
		0x00800000, 0x00806000, 0x0080A000, 0x0080FF00,
		0x00A00000, 0x00A06000, 0x00A0A000, 0x00A0FF00,
		0x00C00000, 0x00C06000, 0x00C0A000, 0x00C0FF00,
		0x00FF0000, 0x00FF6000, 0x00FFA000, 0x00FFFF00,
		0x20000000, 0x20006000, 0x2000A000, 0x2000FF00,
		0x20200000, 0x20206000, 0x2020A000, 0x2020FF00,
		0x20400000, 0x20406000, 0x2040A000, 0x2040FF00,
		0x20600000, 0x20606000, 0x2060A000, 0x2060FF00,
		0x20800000, 0x20806000, 0x2080A000, 0x2080FF00,
		0x20A00000, 0x20A06000, 0x20A0A000, 0x20A0FF00,
		0x20C00000, 0x20C06000, 0x20C0A000, 0x20C0FF00,
		0x20FF0000, 0x20FF6000, 0x20FFA000, 0x20FFFF00,
		0x40000000, 0x40006000, 0x4000A000, 0x4000FF00,
		0x40200000, 0x40206000, 0x4020A000, 0x4020FF00,
		0x40400000, 0x40406000, 0x4040A000, 0x4040FF00,
		0x40600000, 0x40606000, 0x4060A000, 0x4060FF00,
		0x40800000, 0x40806000, 0x4080A000, 0x4080FF00,
		0x40A00000, 0x40A06000, 0x40A0A000, 0x40A0FF00,
		0x40C00000, 0x40C06000, 0x40C0A000, 0x40C0FF00,
		0x40FF0000, 0x40FF6000, 0x40FFA000, 0x40FFFF00,
		0x60000000, 0x60006000, 0x6000A000, 0x6000FF00,
		0x60200000, 0x60206000, 0x6020A000, 0x6020FF00,
		0x60400000, 0x60406000, 0x6040A000, 0x6040FF00,
		0x60600000, 0x60606000, 0x6060A000, 0x6060FF00,
		0x60800000, 0x60806000, 0x6080A000, 0x6080FF00,
		0x60A00000, 0x60A06000, 0x60A0A000, 0x60A0FF00,
		0x60C00000, 0x60C06000, 0x60C0A000, 0x60C0FF00,
		0x60FF0000, 0x60FF6000, 0x60FFA000, 0x60FFFF00,
		0x80000000, 0x80006000, 0x8000A000, 0x8000FF00,
		0x80200000, 0x80206000, 0x8020A000, 0x8020FF00,
		0x80400000, 0x80406000, 0x8040A000, 0x8040FF00,
		0x80600000, 0x80606000, 0x8060A000, 0x8060FF00,
		0x80800000, 0x80806000, 0x8080A000, 0x8080FF00,
		0x80A00000, 0x80A06000, 0x80A0A000, 0x80A0FF00,
		0x80C00000, 0x80C06000, 0x80C0A000, 0x80C0FF00,
		0x80FF0000, 0x80FF6000, 0x80FFA000, 0x80FFFF00,
		0xA0000000, 0xA0006000, 0xA000A000, 0xA000FF00,
		0xA0200000, 0xA0206000, 0xA020A000, 0xA020FF00,
		0xA0400000, 0xA0406000, 0xA040A000, 0xA040FF00,
		0xA0600000, 0xA0606000, 0xA060A000, 0xA060FF00,
		0xA0800000, 0xA0806000, 0xA080A000, 0xA080FF00,
		0xA0A00000, 0xA0A06000, 0xA0A0A000, 0xA0A0FF00,
		0xA0C00000, 0xA0C06000, 0xA0C0A000, 0xA0C0FF00,
		0xA0FF0000, 0xA0FF6000, 0xA0FFA000, 0xA0FFFF00,
		0xC0000000, 0xC0006000, 0xC000A000, 0xC000FF00,
		0xC0200000, 0xC0206000, 0xC020A000, 0xC020FF00,
		0xC0400000, 0xC0406000, 0xC040A000, 0xC040FF00,
		0xC0600000, 0xC0606000, 0xC060A000, 0xC060FF00,
		0xC0800000, 0xC0806000, 0xC080A000, 0xC080FF00,
		0xC0A00000, 0xC0A06000, 0xC0A0A000, 0xC0A0FF00,
		0xC0C00000, 0xC0C06000, 0xC0C0A000, 0xC0C0FF00,
		0xC0FF0000, 0xC0FF6000, 0xC0FFA000, 0xC0FFFF00,
		0xFF000000, 0xFF006000, 0xFF00A000, 0xFF00FF00,
		0xFF200000, 0xFF206000, 0xFF20A000, 0xFF20FF00,
		0xFF400000, 0xFF406000, 0xFF40A000, 0xFF40FF00,
		0xFF600000, 0xFF606000, 0xFF60A000, 0xFF60FF00,
		0xFF800000, 0xFF806000, 0xFF80A000, 0xFF80FF00,
		0xFFA00000, 0xFFA06000, 0xFFA0A000, 0xFFA0FF00,
		0xFFC00000, 0xFFC06000, 0xFFC0A000, 0xFFC0FF00,
		0xFFFF0000, 0xFFFF6000, 0xFFFFA000, 0xFFFFFF00,
	} };

	void setColorBit332(s32 bit, s32 index) noexcept;

/*==================================================================*/

	/**
	 * Original XO-CHIP formula calculated as: 4000Hz * 2^((pitch - 64) / 48) / wavelength
	 * .. where 'wavelength' is 128 bits of the pattern waveform, 'pitch' is 0..255
	 * The step value is the inverse of the playback rate, so it must be divided by a sample rate
	 * Optionally, one can skip part of the formula as: 31.25 * 2^((pitch - 64) / 48)
	 * The entries in the array are pre-calculated (floating-point) frequencies in int format
	 */
	static constexpr std::array<u32, 256> sPitchFreqLUT{{
		0x41466CD5, 0x41494FB1, 0x414C3D4C, 0x414F35CD,
		0x4152395F, 0x4155482A, 0x41586256, 0x415B8812,
		0x415EB985, 0x4161F6DB, 0x41654042, 0x416895E7,
		0x416BF7F5, 0x416F669C, 0x4172E20C, 0x41766A71,
		0x417A0000, 0x417DA2E7, 0x4180A9AC, 0x418288C2,
		0x41846ED1, 0x41865BF2, 0x4188503F, 0x418A4BD3,
		0x418C4EC9, 0x418E593C, 0x41906B49, 0x4192850C,
		0x4194A6A0, 0x4196D025, 0x419901B6, 0x419B3B73,
		0x419D7D79, 0x419FC7E7, 0x41A21ADE, 0x41A4767B,
		0x41A6DAE0, 0x41A9482E, 0x41ABBE84, 0x41AE3E07,
		0x41B0C6D5, 0x41B35915, 0x41B5F4E7, 0x41B89A70,
		0x41BB49D4, 0x41BE0337, 0x41C0C6BF, 0x41C39492,
		0x41C66CD5, 0x41C94FB1, 0x41CC3D4B, 0x41CF35CE,
		0x41D2395F, 0x41D5482A, 0x41D86257, 0x41DB8812,
		0x41DEB984, 0x41E1F6DB, 0x41E54042, 0x41E895E7,
		0x41EBF7F5, 0x41EF669C, 0x41F2E20B, 0x41F66A71,
		0x41FA0000, 0x41FDA2E7, 0x4200A9AC, 0x420288C2,
		0x42046ED2, 0x42065BF2, 0x4208503F, 0x420A4BD3,
		0x420C4EC9, 0x420E593C, 0x42106B49, 0x4212850B,
		0x4214A6A0, 0x4216D026, 0x421901B6, 0x421B3B73,
		0x421D7D79, 0x421FC7E7, 0x42221ADE, 0x4224767B,
		0x4226DAE0, 0x4229482E, 0x422BBE85, 0x422E3E07,
		0x4230C6D5, 0x42335914, 0x4235F4E7, 0x42389A70,
		0x423B49D4, 0x423E0337, 0x4240C6BF, 0x42439492,
		0x42466CD5, 0x42494FB1, 0x424C3D4B, 0x424F35CE,
		0x4252395F, 0x4255482A, 0x42586257, 0x425B8812,
		0x425EB984, 0x4261F6DC, 0x42654042, 0x426895E6,
		0x426BF7F5, 0x426F669C, 0x4272E20B, 0x42766A72,
		0x427A0000, 0x427DA2E7, 0x4280A9AC, 0x428288C2,
		0x42846ED2, 0x42865BF2, 0x4288503F, 0x428A4BD4,
		0x428C4EC9, 0x428E593C, 0x42906B49, 0x4292850B,
		0x4294A6A0, 0x4296D026, 0x429901B6, 0x429B3B73,
		0x429D7D79, 0x429FC7E7, 0x42A21ADE, 0x42A4767B,
		0x42A6DAE0, 0x42A9482E, 0x42ABBE85, 0x42AE3E06,
		0x42B0C6D5, 0x42B35915, 0x42B5F4E7, 0x42B89A70,
		0x42BB49D4, 0x42BE0336, 0x42C0C6BF, 0x42C39492,
		0x42C66CD5, 0x42C94FB1, 0x42CC3D4C, 0x42CF35CD,
		0x42D2395F, 0x42D5482A, 0x42D86256, 0x42DB8812,
		0x42DEB985, 0x42E1F6DB, 0x42E54042, 0x42E895E7,
		0x42EBF7F5, 0x42EF669C, 0x42F2E20C, 0x42F66A71,
		0x42FA0000, 0x42FDA2E7, 0x4300A9AC, 0x430288C2,
		0x43046ED1, 0x43065BF3, 0x4308503F, 0x430A4BD3,
		0x430C4ECA, 0x430E593C, 0x43106B48, 0x4312850C,
		0x4314A6A0, 0x4316D025, 0x431901B7, 0x431B3B73,
		0x431D7D78, 0x431FC7E8, 0x43221ADE, 0x4324767A,
		0x4326DAE1, 0x4329482E, 0x432BBE84, 0x432E3E07,
		0x4330C6D5, 0x43335914, 0x4335F4E8, 0x43389A70,
		0x433B49D3, 0x433E0338, 0x4340C6BF, 0x43439491,
		0x43466CD6, 0x43494FB1, 0x434C3D4A, 0x434F35CE,
		0x4352395F, 0x43554829, 0x43586258, 0x435B8812,
		0x435EB983, 0x4361F6DC, 0x43654042, 0x436895E6,
		0x436BF7F6, 0x436F669C, 0x4372E20A, 0x43766A72,
		0x437A0000, 0x437DA2E7, 0x4380A9AC, 0x438288C2,
		0x43846ED1, 0x43865BF3, 0x4388503F, 0x438A4BD3,
		0x438C4ECA, 0x438E593C, 0x43906B48, 0x4392850C,
		0x4394A6A0, 0x4396D025, 0x439901B7, 0x439B3B73,
		0x439D7D78, 0x439FC7E8, 0x43A21ADE, 0x43A4767A,
		0x43A6DAE1, 0x43A9482E, 0x43ABBE84, 0x43AE3E07,
		0x43B0C6D5, 0x43B35914, 0x43B5F4E8, 0x43B89A70,
		0x43BB49D3, 0x43BE0338, 0x43C0C6BF, 0x43C39491,
		0x43C66CD6, 0x43C94FB1, 0x43CC3D4A, 0x43CF35CE,
		0x43D2395F, 0x43D54829, 0x43D86258, 0x43DB8812,
		0x43DEB983, 0x43E1F6DC, 0x43E54042, 0x43E895E6,
		0x43EBF7F6, 0x43EF669C, 0x43F2E20A, 0x43F66A72,
	}};

	using PatternData = std::array<u8, 16>;
	static inline thread_local PatternData mPattern{{
		0x0F, 0x00,	0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00,
		0x0F, 0x00,	0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00,
	}};

	void setPatternPitch(s32 pitch) noexcept;

	static void makePatternWave(f32* data, u32 size, Voice* voice, Stream*) noexcept;

/*==================================================================*/

	static inline thread_local u32 mPlanarMask{ 0x1 };

	Map2D<u8> mDisplayBuffer[4];

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

	u32 incIndexRegister(u32 value) noexcept override final
		{ return mRegisterI = (mRegisterI + value) & 0xFFFF; }

	u32 decIndexRegister(u32 value) noexcept override final
		{ return mRegisterI = (mRegisterI - value) & 0xFFFF; }

	u32 setIndexRegister(u32 value) noexcept override final
		{ return mRegisterI = value & 0xFFFF; }

/*==================================================================*/

	auto NNNN() const noexcept { return mMemoryBank[mCurrentPC] << 8 | mMemoryBank[mCurrentPC + 1]; }

public:
	XOCHIP()
		: mDisplayBuffer{
			{cScreenSizeX, cScreenSizeY},
			{cScreenSizeX, cScreenSizeY},
			{cScreenSizeX, cScreenSizeY},
			{cScreenSizeX, cScreenSizeY},
		}
	{}

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

/*==================================================================*/
	#pragma region 0 instruction branch

	// 00DN - scroll selected color plane N lines down
	void instruction_00CN(s32 N) noexcept;
	// 00DN - scroll selected color plane N lines up
	void instruction_00DN(s32 N) noexcept;
	// 00E0 - erase selected color plane
	void instruction_00E0() noexcept;
	// 00EE - return from subroutine
	void instruction_00EE() noexcept;
	// 00FB - scroll selected color plane 4 pixels right
	void instruction_00FB() noexcept;
	// 00FC - scroll selected color plane 4 pixels left
	void instruction_00FC() noexcept;
	// 00FD - stop signal
	void instruction_00FD() noexcept;
	// 00FE - display res == 64x32, erase whole display
	void instruction_00FE() noexcept;
	// 00FF - display res == 128x64, erase whole display
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
	// 5XY2 - store range of registers to memory
	void instruction_5xy2(s32 X, s32 Y) noexcept;
	// 5XY3 - load range of registers from memory
	void instruction_5xy3(s32 X, s32 Y) noexcept;
	// 5XY4 - load range of colors from memory *EXPERIMENTAL*
	void instruction_5xy4(s32 X, s32 Y) noexcept;

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

	// BNNN - jump to NNN + V0
	void instruction_BNNN(s32 NNN) noexcept;

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

	void drawByte(s32 X, s32 Y, s32 P, u32 DATA) noexcept;

	enum Plane {
		P0, P1, P2, P3,
		P0M = 1 << P0,
		P1M = 1 << P1,
		P2M = 1 << P2,
		P3M = 1 << P3,
	};

	// For planar mask N (0000 to 1111), count set bits
	// to the right of bit (currently drawn plane) 0..3
	static constexpr u8 sPlaneMult[4][16] = {
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, // Plane 0
		{0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1}, // Plane 1
		{0,1,1,2,0,1,1,2,0,1,1,2,0,1,1,2}, // Plane 2
		{0,1,1,2,1,2,2,3,0,1,1,2,1,2,2,3}, // Plane 3
	};

	template <std::size_t P>
	void drawSingleRow(s32 X, s32 Y) noexcept;

	template <std::size_t P>
	void drawDoubleRow(s32 X, s32 Y) noexcept;

	template <std::size_t P>
	void drawMultiRow (s32 X, s32 Y, s32 N) noexcept;

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

	// F000 - set I = NEXT NNNN then skip instruction
	void instruction_F000() noexcept;
	// F002 - load 16-byte audio pattern from RAM at I
	void instruction_F002() noexcept;
	// FN01 - set plane drawing to N
	void instruction_FN01(s32 N) noexcept;
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
	// FX3A - set sound pitch = VX
	void instruction_Fx3A(s32 X) noexcept;
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
