/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "CHIP8_MODERN.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_CHIP8_MODERN)

#include "BasicVideoSpec.hpp"
#include "GlobalAudioBase.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(CHIP8_MODERN, ".ch8")

/*==================================================================*/

CHIP8_MODERN::CHIP8_MODERN() {
	::fill_n(mMemoryBank, cTotalMemory, cSafezoneOOB, 0xFF);

	copyGameToMemory(mMemoryBank.data() + cGameLoadPos);
	copyFontToMemory(mMemoryBank.data(), cLargeFontOffset);

	mDisplay.set(cScreenSizeX, cScreenSizeY);
	setViewportSizes(true, cScreenSizeX, cScreenSizeY, cResSizeMult, 2);
	setBaseSystemFramerate(cRefreshRate);

	mVoices[VOICE::ID_0].userdata = &mAudioTimers[VOICE::ID_0];
	mVoices[VOICE::ID_1].userdata = &mAudioTimers[VOICE::ID_1];
	mVoices[VOICE::ID_2].userdata = &mAudioTimers[VOICE::ID_2];
	mVoices[VOICE::ID_3].userdata = &mAudioTimers[VOICE::ID_3];

	mCurrentPC = cStartOffset;
	mTargetCPF = Quirk.waitVblank ? cInstSpeedHi : cInstSpeedLo;
}

/*==================================================================*/

void CHIP8_MODERN::instructionLoop() noexcept {

	auto cycleCount{ 0 };
	for (; cycleCount < mTargetCPF; ++cycleCount) {
		const auto HI{ mMemoryBank[mCurrentPC + 0u] };
		const auto LO{ mMemoryBank[mCurrentPC + 1u] };
		nextInstruction();

		switch (HI >> 4) {
			case 0x0:
				switch (HI << 8 | LO) {
					case 0x00E0:
						instruction_00E0();
						break;
					case 0x00EE:
						instruction_00EE();
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
			case 0x1:
				instruction_1NNN(HI << 8 | LO);
				break;
			case 0x2:
				instruction_2NNN(HI << 8 | LO);
				break;
			case 0x3:
				instruction_3xNN(HI & 0xF, LO);
				break;
			case 0x4:
				instruction_4xNN(HI & 0xF, LO);
				break;
			case 0x5:
				if (LO & 0xF) [[unlikely]] {
					instructionError(HI, LO);
				} else {
					instruction_5xy0(HI & 0xF, LO >> 4);
				}
				break;
			case 0x6:
				instruction_6xNN(HI & 0xF, LO);
				break;
			case 0x7:
				instruction_7xNN(HI & 0xF, LO);
				break;
			case 0x8:
				switch (LO & 0xF) {
					case 0x0:
						instruction_8xy0(HI & 0xF, LO >> 4);
						break;
					case 0x1:
						instruction_8xy1(HI & 0xF, LO >> 4);
						break;
					case 0x2:
						instruction_8xy2(HI & 0xF, LO >> 4);
						break;
					case 0x3:
						instruction_8xy3(HI & 0xF, LO >> 4);
						break;
					case 0x4:
						instruction_8xy4(HI & 0xF, LO >> 4);
						break;
					case 0x5:
						instruction_8xy5(HI & 0xF, LO >> 4);
						break;
					case 0x7:
						instruction_8xy7(HI & 0xF, LO >> 4);
						break;
					case 0x6:
						instruction_8xy6(HI & 0xF, LO >> 4);
						break;
					case 0xE:
						instruction_8xyE(HI & 0xF, LO >> 4);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
			case 0x9:
				if (LO & 0xF) [[unlikely]] {
					instructionError(HI, LO);
				} else {
					instruction_9xy0(HI & 0xF, LO >> 4);
				}
				break;
			case 0xA:
				instruction_ANNN(HI << 8 | LO);
				break;
			case 0xB:
				instruction_BNNN(HI << 8 | LO);
				break;
			case 0xC:
				instruction_CxNN(HI & 0xF, LO);
				break;
			[[likely]]
			case 0xD:
				instruction_DxyN(HI & 0xF, LO >> 4, LO & 0xF);
				break;
			case 0xE:
				switch (LO) {
					case 0x9E:
						instruction_Ex9E(HI & 0xF);
						break;
					case 0xA1:
						instruction_ExA1(HI & 0xF);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
			case 0xF:
				switch (LO) {
					case 0x07:
						instruction_Fx07(HI & 0xF);
						break;
					case 0x0A:
						instruction_Fx0A(HI & 0xF);
						break;
					case 0x15:
						instruction_Fx15(HI & 0xF);
						break;
					case 0x18:
						instruction_Fx18(HI & 0xF);
						break;
					case 0x1E:
						instruction_Fx1E(HI & 0xF);
						break;
					case 0x29:
						instruction_Fx29(HI & 0xF);
						break;
					case 0x33:
						instruction_Fx33(HI & 0xF);
						break;
					case 0x55:
						instruction_FN55(HI & 0xF);
						break;
					case 0x65:
						instruction_FN65(HI & 0xF);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
		}
	}
}

void CHIP8_MODERN::renderAudioData() {
	mixAudioData({
		{ makePulseWave, &mVoices[VOICE::ID_0] },
		{ makePulseWave, &mVoices[VOICE::ID_1] },
		{ makePulseWave, &mVoices[VOICE::ID_2] },
		{ makePulseWave, &mVoices[VOICE::BUZZER] },
	});

	setDisplayBorderColor(sBitColors[!!::accumulate(mAudioTimers)]);
}

void CHIP8_MODERN::renderVideoData() {
	BVS->displayBuffer.write(mDisplayBuffer, isUsingPixelTrails()
		? [](u32 pixel) noexcept
			{ return sBitColors[pixel != 0] | cPixelOpacity[pixel]; }
		: [](u32 pixel) noexcept
			{ return sBitColors[pixel >> 3] | 0xFFu; }
	);

	std::for_each(EXEC_POLICY(unseq)
		mDisplayBuffer.begin(),
		mDisplayBuffer.end(),
		[](auto& pixel) noexcept
			{ ::assign_cast(pixel, (pixel & 0x8) | (pixel >> 1)); }
	);
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void CHIP8_MODERN::instruction_00E0() noexcept {
		if (Quirk.waitVblank) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
		::fill(mDisplayBuffer);
	}
	void CHIP8_MODERN::instruction_00EE() noexcept {
		mCurrentPC = mStackBank[--mStackTop & 0xF];
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void CHIP8_MODERN::instruction_1NNN(s32 NNN) noexcept {
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void CHIP8_MODERN::instruction_2NNN(s32 NNN) noexcept {
		mStackBank[mStackTop++ & 0xF] = mCurrentPC;
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void CHIP8_MODERN::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] == NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void CHIP8_MODERN::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] != NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void CHIP8_MODERN::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] == mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void CHIP8_MODERN::instruction_6xNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void CHIP8_MODERN::instruction_7xNN(s32 X, s32 NN) noexcept {
		::assign_cast_add(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void CHIP8_MODERN::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8_MODERN::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8_MODERN::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8_MODERN::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8_MODERN::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum{ mRegisterV[X] + mRegisterV[Y] };
		::assign_cast(mRegisterV[X], sum);
		::assign_cast(mRegisterV[0xF], sum >> 8);
	}
	void CHIP8_MODERN::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[X] >= mRegisterV[Y] };
		::assign_cast_sub(mRegisterV[X], mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void CHIP8_MODERN::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[Y] >= mRegisterV[X] };
		::assign_cast_rsub(mRegisterV[X], mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void CHIP8_MODERN::instruction_8xy6(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { ::assign_cast(mRegisterV[X], mRegisterV[Y]); }
		const bool lsb{ (mRegisterV[X] & 0x01) != 0 };
		::assign_cast_shr(mRegisterV[X], 1);
		::assign_cast(mRegisterV[0xF], lsb);
	}
	void CHIP8_MODERN::instruction_8xyE(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { ::assign_cast(mRegisterV[X], mRegisterV[Y]); }
		const bool msb{ (mRegisterV[X] & 0x80) != 0 };
		::assign_cast_shl(mRegisterV[X], 1);
		::assign_cast(mRegisterV[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void CHIP8_MODERN::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] != mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void CHIP8_MODERN::instruction_ANNN(s32 NNN) noexcept {
		::assign_cast(mRegisterI, NNN & 0xFFF);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void CHIP8_MODERN::instruction_BNNN(s32 NNN) noexcept {
		performProgJump(NNN + mRegisterV[0]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void CHIP8_MODERN::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void CHIP8_MODERN::drawByte(s32 X, s32 Y, u32 DATA) noexcept {
		switch (DATA) {
			[[unlikely]]
			case 0b00000000:
				return;

			[[likely]]
			case 0b10000000:
				if (Quirk.wrapSprite) { X %= cScreenSizeX; }
				if (X < cScreenSizeX) {
					if (!((mDisplayBuffer(X, Y) ^= 0x8) & 0x8))
						[[unlikely]] { mRegisterV[0xF] = 1; }
				}
				return;

			[[unlikely]]
			default:
				if (Quirk.wrapSprite) { X %= cScreenSizeX; }
				else if (X >= cScreenSizeX) { return; }

				for (auto B{ 0 }; B < 8; ++B, ++X %= cScreenSizeX) {
					if (DATA & 0x80 >> B) {
						if (!((mDisplayBuffer(X, Y) ^= 0x8) & 0x8))
							[[unlikely]] { mRegisterV[0xF] = 1; }
					}
					if (!Quirk.wrapSprite && X == cScreenSizeX - 1) { return; }
				}
				return;
		}
	}

	void CHIP8_MODERN::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		if (Quirk.waitVblank) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }

		auto pX{ mRegisterV[X] % cScreenSizeX };
		auto pY{ mRegisterV[Y] % cScreenSizeY };

		mRegisterV[0xF] = 0;

		switch (N) {
			[[likely]]
			case 1:
				drawByte(pX, pY, readMemoryI(0));
				break;

			[[unlikely]]
			case 0:
				for (auto H{ 0 }, I{ 0 }; H < 16; ++H, I += 2, ++pY %= cScreenSizeY)
				{
					drawByte(pX + 0, pY, readMemoryI(I + 0));
					drawByte(pX + 8, pY, readMemoryI(I + 1));
					
					if (!Quirk.wrapSprite && pY == cScreenSizeY - 1) { break; }
				}
				break;

			[[unlikely]]
			default:
				for (auto H{ 0 }; H < N; ++H, ++pY %= cScreenSizeY)
				{
					drawByte(pX, pY, readMemoryI(H));
					if (!Quirk.wrapSprite && pY == cScreenSizeY - 1) { break; }
				}
				break;
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void CHIP8_MODERN::instruction_Ex9E(s32 X) noexcept {
		if (keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}
	void CHIP8_MODERN::instruction_ExA1(s32 X) noexcept {
		if (!keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void CHIP8_MODERN::instruction_Fx07(s32 X) noexcept {
		::assign_cast(mRegisterV[X], mDelayTimer);
	}
	void CHIP8_MODERN::instruction_Fx0A(s32 X) noexcept {
		triggerInterrupt(Interrupt::INPUT);
		mInputReg = &mRegisterV[X];
	}
	void CHIP8_MODERN::instruction_Fx15(s32 X) noexcept {
		::assign_cast(mDelayTimer, mRegisterV[X]);
	}
	void CHIP8_MODERN::instruction_Fx18(s32 X) noexcept {
		startVoice(mRegisterV[X] + (mRegisterV[X] == 1));
	}
	void CHIP8_MODERN::instruction_Fx1E(s32 X) noexcept {
		::assign_cast_add(mRegisterI, mRegisterV[X]);
		::assign_cast_and(mRegisterI, 0xFFF);
	}
	void CHIP8_MODERN::instruction_Fx29(s32 X) noexcept {
		::assign_cast(mRegisterI, (mRegisterV[X] & 0xF) * 5 + cSmallFontOffset);
	}
	void CHIP8_MODERN::instruction_Fx33(s32 X) noexcept {
		const auto N__{ mRegisterV[X] * 0x51EB851Full >> 37 };
		const auto _NN{ mRegisterV[X] - N__ * 100 };
		const auto _N_{ _NN * 0xCCCDull >> 19 };
		const auto __N{ _NN - _N_ * 10 };

		writeMemoryI(N__, 0);
		writeMemoryI(_N_, 1);
		writeMemoryI(__N, 2);
	}
	void CHIP8_MODERN::instruction_FN55(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { writeMemoryI(mRegisterV[idx], idx); }
		mRegisterI = !Quirk.idxRegNoInc ? (mRegisterI + N + 1) & 0xFFF : mRegisterI;
		//if (!Quirk.idxRegNoInc) [[likely]] { mRegisterI = (mRegisterI + N + 1) & 0xFFF; }
	}
	void CHIP8_MODERN::instruction_FN65(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { mRegisterV[idx] = readMemoryI(idx); }
		mRegisterI = !Quirk.idxRegNoInc ? (mRegisterI + N + 1) & 0xFFF : mRegisterI;
		//if (!Quirk.idxRegNoInc) [[likely]] { mRegisterI = (mRegisterI + N + 1) & 0xFFF; }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
