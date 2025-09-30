/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "CHIP8X.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_CHIP8X)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(CHIP8X, ".c8x")

/*==================================================================*/

void CHIP8X::initializeSystem() noexcept {
	::fill_n(mMemoryBank, cTotalMemory, cSafezoneOOB, 0xFF);

	copyGameToMemory(mMemoryBank.data() + cGameLoadPos);
	copyFontToMemory(mMemoryBank.data(), 80);

	mDisplay.set(cScreenSizeX, cScreenSizeY);
	setViewportSizes(true, cScreenSizeX, cScreenSizeY, cResSizeMult, 2);
	setBaseSystemFramerate(cRefreshRate);

	mVoices[VOICE::UNIQUE].userdata = &mAudioTimers[VOICE::UNIQUE];
	mVoices[VOICE::BUZZER].userdata = &mAudioTimers[VOICE::BUZZER];

	mCurrentPC = cStartOffset;
	mTargetCPF = cInstSpeedHi;

	// test first color rect as the original hardware did
	mColoredBuffer(0, 0) = cForeColor[2];
}

void CHIP8X::handleCycleLoop() noexcept
	{ LOOP_DISPATCH(instructionLoop); }

template <typename Lambda>
void CHIP8X::instructionLoop(Lambda&& condition) noexcept {
	for (mCycleCount = 0; condition(); ++mCycleCount) {
		const auto HI{ mMemoryBank[mCurrentPC++] };
		const auto LO{ mMemoryBank[mCurrentPC++] };

		#define _NNN (HI << 8 | LO)
		#define _X (HI & 0xF)
		#define Y_ (LO >> 4)
		#define _N (LO & 0xF)

		switch (HI) {
			case 0x00:
				switch (LO) {
					case 0xE0:
						instruction_00E0();
						break;
					case 0xEE:
						instruction_00EE();
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
			case 0x02:
				if (LO){
					instruction_02A0();
				} else [[unlikely]] {
					instructionError(HI, LO);
				}
				break;
			CASE_xNF(0x10):
				instruction_1NNN(_NNN);
				break;
			CASE_xNF(0x20):
				instruction_2NNN(_NNN);
				break;
			CASE_xNF(0x30):
				instruction_3xNN(_X, LO);
				break;
			CASE_xNF(0x40):
				instruction_4xNN(_X, LO);
				break;
			CASE_xNF(0x50):
				switch (LO) {
					CASE_xFN(0x00):
						instruction_5xy0(_X, Y_);
						break;
					CASE_xFN(0x01):
						instruction_5xy1(_X, Y_);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
			CASE_xNF(0x60):
				instruction_6xNN(_X, LO);
				break;
			CASE_xNF(0x70):
				instruction_7xNN(_X, LO);
				break;
			CASE_xNF(0x80):
				switch (LO) {
					CASE_xFN(0x0):
						instruction_8xy0(_X, Y_);
						break;
					CASE_xFN(0x1):
						instruction_8xy1(_X, Y_);
						break;
					CASE_xFN(0x2):
						instruction_8xy2(_X, Y_);
						break;
					CASE_xFN(0x3):
						instruction_8xy3(_X, Y_);
						break;
					CASE_xFN(0x4):
						instruction_8xy4(_X, Y_);
						break;
					CASE_xFN(0x5):
						instruction_8xy5(_X, Y_);
						break;
					CASE_xFN(0x7):
						instruction_8xy7(_X, Y_);
						break;
					CASE_xFN(0x6):
						instruction_8xy6(_X, Y_);
						break;
					CASE_xFN(0xE):
						instruction_8xyE(_X, Y_);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
			CASE_xNF(0x90):
				if (_N) [[unlikely]] {
					instructionError(HI, LO);
				} else {
					instruction_9xy0(_X, Y_);
				}
				break;
			CASE_xNF(0xA0):
				instruction_ANNN(_NNN);
				break;
			CASE_xNF(0xB0):
				if (HI == 0xBF) [[unlikely]] {
					instructionError(HI, LO);
				} else {
					instruction_BxyN(_X, Y_, _N);
				}
				break;
			CASE_xNF(0xC0):
				instruction_CxNN(_X, LO);
				break;
			[[likely]]
			CASE_xNF(0xD0):
				instruction_DxyN(_X, Y_, _N);
				break;
			CASE_xNF(0xE0):
				switch (LO) {
					case 0x9E:
						instruction_Ex9E(_X);
						break;
					case 0xA1:
						instruction_ExA1(_X);
						break;
					case 0xF2:
						instruction_ExF2(_X);
						break;
					case 0xF5:
						instruction_ExF5(_X);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
			CASE_xNF(0xF0):
				switch (LO) {
					case 0x07:
						instruction_Fx07(_X);
						break;
					case 0x0A:
						instruction_Fx0A(_X);
						break;
					case 0x15:
						instruction_Fx15(_X);
						break;
					case 0x18:
						instruction_Fx18(_X);
						break;
					case 0x1E:
						instruction_Fx1E(_X);
						break;
					case 0x29:
						instruction_Fx29(_X);
						break;
					case 0x33:
						instruction_Fx33(_X);
						break;
					case 0x55:
						instruction_FN55(_X);
						break;
					case 0x65:
						instruction_FN65(_X);
						break;
					case 0xF8:
						instruction_FxF8(_X);
						break;
					case 0xFB:
						instruction_FxFB(_X);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
		}
	}
}

void CHIP8X::renderAudioData() {
	mixAudioData({
		{ makePulseWave, &mVoices[VOICE::UNIQUE] },
		{ makePulseWave, &mVoices[VOICE::BUZZER] },
	});

	static constexpr u32 idx[]{ 2, 7, 4, 1 };
	setDisplayBorderColor(::accumulate(mAudioTimers)
		? cForeColor[idx[mBackgroundColor]] : cBackColor[mBackgroundColor]);
}

void CHIP8X::renderVideoData() {
	if (isUsingPixelTrails()) {
		BVS->displayBuffer.write(mDisplayBuffer, [this](auto& pixel) noexcept {
			if (pixel == 0) {
				return cBackColor[mBackgroundColor] | 0xFFu;
			} else {
				const auto idx{ &pixel - mDisplayBuffer.data() };
				const auto Y{ (idx / cScreenSizeX) & mColorResolution };
				const auto X{ (idx % cScreenSizeX) >> 3 };
				return mColoredBuffer(X, Y) | cPixelOpacity[pixel];
			}
		});

		std::for_each(EXEC_POLICY(unseq)
			mDisplayBuffer.begin(), mDisplayBuffer.end(),
			[](auto& pixel) noexcept { ::assign_cast(pixel, (pixel & 0x8) | (pixel >> 1)); }
		);
	} else {
		BVS->displayBuffer.write(mDisplayBuffer, [this](auto& pixel) noexcept {
			if (pixel == 0) {
				return cBackColor[mBackgroundColor] | 0xFFu;
			} else {
				const auto idx{ &pixel - mDisplayBuffer.data() };
				const auto Y{ (idx / cScreenSizeX) & mColorResolution };
				const auto X{ (idx % cScreenSizeX) >> 3 };
				return mColoredBuffer(X, Y) | 0xFFu;
			}
		});
	}
}

void CHIP8X::setBuzzerPitch(s32 pitch) noexcept {
	if (auto* stream{ mAudioDevice.at(STREAM::MAIN) }) {
		mVoices[VOICE::UNIQUE].setStep((sTonalOffset + (
			(0xFF - (pitch ? pitch : 0x80)) >> 3 << 4)
		) / stream->getFreq() * getFramerateMultiplier());
	}
}

void CHIP8X::drawLoresColor(s32 X, s32 Y, s32 idx) noexcept {
	for (auto pY{ 0 }, maxH{ Y >> 4 }; pY <= maxH; ++pY) {
		for (auto pX{ 0 }, maxW{ X >> 4 }; pX <= maxW; ++pX) {
			mColoredBuffer((X + pX) & 0x7, ((Y + pY) << 2) & 0x1F) = cForeColor[idx & 0x7];
		}
	}
	mColorResolution = 0xFC;
}

void CHIP8X::drawHiresColor(s32 X, s32 Y, s32 idx, s32 N) noexcept {
	for (auto pY{ Y }, pX{ X >> 3 }; pY < Y + N; ++pY) {
		mColoredBuffer(pX & 0x7, pY & 0x1F) = cForeColor[idx & 0x7];
	}
	mColorResolution = 0xFF;
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void CHIP8X::instruction_00E0() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		::fill(mDisplayBuffer);
	}
	void CHIP8X::instruction_00EE() noexcept {
		mCurrentPC = mStackBank[--mStackTop & 0xF];
	}
	void CHIP8X::instruction_02A0() noexcept {
		setDisplayBorderColor(cBackColor[++mBackgroundColor &= 0x3]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void CHIP8X::instruction_1NNN(s32 NNN) noexcept {
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void CHIP8X::instruction_2NNN(s32 NNN) noexcept {
		mStackBank[mStackTop++ & 0xF] = mCurrentPC;
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void CHIP8X::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] == NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void CHIP8X::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] != NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void CHIP8X::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] == mRegisterV[Y]) { skipInstruction(); }
	}
	void CHIP8X::instruction_5xy1(s32 X, s32 Y) noexcept {
		const auto lenX{ (mRegisterV[X] & 0x70) + (mRegisterV[Y] & 0x70) };
		const auto lenY{ (mRegisterV[X] + mRegisterV[Y]) & 0x7 };
		::assign_cast(mRegisterV[X], lenX | lenY);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void CHIP8X::instruction_6xNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void CHIP8X::instruction_7xNN(s32 X, s32 NN) noexcept {
		::assign_cast_add(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void CHIP8X::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8X::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8X::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8X::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(mRegisterV[X], mRegisterV[Y]);
	}
	void CHIP8X::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum{ mRegisterV[X] + mRegisterV[Y] };
		::assign_cast(mRegisterV[X], sum);
		::assign_cast(mRegisterV[0xF], sum >> 8);
	}
	void CHIP8X::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[X] >= mRegisterV[Y] };
		::assign_cast(mRegisterV[X], mRegisterV[X] - mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void CHIP8X::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[Y] >= mRegisterV[X] };
		::assign_cast(mRegisterV[X], mRegisterV[Y] - mRegisterV[X]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void CHIP8X::instruction_8xy6(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { mRegisterV[X] = mRegisterV[Y]; }
		const bool lsb{ (mRegisterV[X] & 1) == 1 };
		::assign_cast(mRegisterV[X], mRegisterV[X] >> 1);
		::assign_cast(mRegisterV[0xF], lsb);
	}
	void CHIP8X::instruction_8xyE(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { mRegisterV[X] = mRegisterV[Y]; }
		const bool msb{ (mRegisterV[X] >> 7) == 1 };
		::assign_cast(mRegisterV[X], mRegisterV[X] << 1);
		::assign_cast(mRegisterV[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void CHIP8X::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] != mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void CHIP8X::instruction_ANNN(s32 NNN) noexcept {
		setIndexRegister(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void CHIP8X::instruction_BxyN(s32 X, s32 Y, s32 N) noexcept {
		if (N) {
			drawHiresColor(mRegisterV[X], mRegisterV[X + 1], mRegisterV[Y] & 0x7, N);
		} else {
			drawLoresColor(mRegisterV[X], mRegisterV[X + 1], mRegisterV[Y] & 0x7);
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void CHIP8X::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void CHIP8X::drawByte(s32 X, s32 Y, u32 DATA) noexcept {
		switch (DATA) {
			[[unlikely]]
			case 0b00000000:
				return;

			[[likely]]
			case 0b10000000:
				if (Quirk.wrapSprite) { X &= (cScreenSizeX - 1); }
				if (X < cScreenSizeX) {
					if (!((mDisplayBuffer[Y * cScreenSizeX + X] ^= 0x8) & 0x8))
						{ mRegisterV[0xF] = 1; }
				}
				return;

			[[unlikely]]
			default:
				if (Quirk.wrapSprite) { X &= (cScreenSizeX - 1); }
				else if (X >= cScreenSizeX) { return; }

				for (auto B{ 0 }; B < 8; ++B, ++X &= (cScreenSizeX - 1)) {
					if (DATA & 0x80 >> B) {
						if (!((mDisplayBuffer[Y * cScreenSizeX + X] ^= 0x8) & 0x8))
							{ mRegisterV[0xF] = 1; }
					}
					if (!Quirk.wrapSprite && X == (cScreenSizeX - 1)) { return; }
				}
				return;
		}
	}

	void CHIP8X::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		triggerInterrupt(Interrupt::FRAME);

		auto pX{ mRegisterV[X] & (cScreenSizeX - 1) };
		auto pY{ mRegisterV[Y] & (cScreenSizeY - 1) };

		mRegisterV[0xF] = 0;

		switch (N) {
			[[likely]]
			case 1:
				drawByte(pX, pY, readMemoryI(0));
				break;

			[[unlikely]]
			case 0:
				for (auto H{ 0 }, I{ 0 }; H < 16; ++H, I += 2, ++pY &= (cScreenSizeY - 1))
				{
					drawByte(pX + 0, pY, readMemoryI(I + 0));
					drawByte(pX + 8, pY, readMemoryI(I + 1));

					if (!Quirk.wrapSprite && pY == (cScreenSizeY - 1)) { break; }
				}
				break;

			[[unlikely]]
			default:
				for (auto H{ 0 }; H < N; ++H, ++pY &= (cScreenSizeY - 1))
				{
					drawByte(pX, pY, readMemoryI(H));
					if (!Quirk.wrapSprite && pY == (cScreenSizeY - 1)) { break; }
				}
				break;
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void CHIP8X::instruction_Ex9E(s32 X) noexcept {
		if (keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}
	void CHIP8X::instruction_ExA1(s32 X) noexcept {
		if (!keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}
	void CHIP8X::instruction_ExF2(s32 X) noexcept {
		if (keyHeld_P2(mRegisterV[X])) { skipInstruction(); }
	}
	void CHIP8X::instruction_ExF5(s32 X) noexcept {
		if (!keyHeld_P2(mRegisterV[X])) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void CHIP8X::instruction_Fx07(s32 X) noexcept {
		::assign_cast(mRegisterV[X], mDelayTimer);
	}
	void CHIP8X::instruction_Fx0A(s32 X) noexcept {
		triggerInterrupt(Interrupt::INPUT);
		mInputReg = &mRegisterV[X];
	}
	void CHIP8X::instruction_Fx15(s32 X) noexcept {
		mDelayTimer = mRegisterV[X];
	}
	void CHIP8X::instruction_Fx18(s32 X) noexcept {
		mAudioTimers[VOICE::UNIQUE].set(mRegisterV[X] + (mRegisterV[X] == 1));
	}
	void CHIP8X::instruction_Fx1E(s32 X) noexcept {
		incIndexRegister(mRegisterV[X]);
	}
	void CHIP8X::instruction_Fx29(s32 X) noexcept {
		setIndexRegister((mRegisterV[X] & 0xF) * 5 + cSmallFontOffset);
	}
	void CHIP8X::instruction_Fx33(s32 X) noexcept {
		const TriBCD bcd{ mRegisterV[X] };

		writeMemoryI(bcd.digit[2], 0);
		writeMemoryI(bcd.digit[1], 1);
		writeMemoryI(bcd.digit[0], 2);
	}
	void CHIP8X::instruction_FN55(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { writeMemoryI(mRegisterV[idx], idx); }
		if (!Quirk.idxRegNoInc) [[likely]] { incIndexRegister(N + 1); }
	}
	void CHIP8X::instruction_FN65(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { mRegisterV[idx] = readMemoryI(idx); }
		if (!Quirk.idxRegNoInc) [[likely]] { incIndexRegister(N + 1); }
	}
	void CHIP8X::instruction_FxF8(s32 X) noexcept {
		setBuzzerPitch(mRegisterV[X]);
	}
	void CHIP8X::instruction_FxFB(s32) noexcept {
		triggerInterrupt(Interrupt::FRAME);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
