/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "SCHIP_LEGACY.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_SCHIP_LEGACY)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(SCHIP_LEGACY, ".sc8")

/*==================================================================*/

void SCHIP_LEGACY::initializeSystem() noexcept {
	::generate_n(mMemoryBank, 0, cTotalMemory,
		[&]() noexcept { return RNG->next<u8>(); });
	::fill_n(mMemoryBank, cTotalMemory, cSafezoneOOB, 0xFF);

	copyGameToMemory(mMemoryBank.data() + cGameLoadPos);
	copyFontToMemory(mMemoryBank.data(), 180);

	mDisplay.set(cScreenSizeX, cScreenSizeY);
	setViewportSizes(true, cScreenSizeX, cScreenSizeY, cResSizeMult, 2);
	setBaseSystemFramerate(cRefreshRate);

	mVoices[VOICE::ID_0].userdata = &mAudioTimers[VOICE::ID_0];
	mVoices[VOICE::ID_1].userdata = &mAudioTimers[VOICE::ID_1];
	mVoices[VOICE::ID_2].userdata = &mAudioTimers[VOICE::ID_2];
	mVoices[VOICE::ID_3].userdata = &mAudioTimers[VOICE::ID_3];

	mCurrentPC = cStartOffset;

	prepDisplayArea(Resolution::LO);
}

void SCHIP_LEGACY::handleCycleLoop() noexcept
	{ LOOP_DISPATCH(instructionLoop); }

template <typename Lambda>
void SCHIP_LEGACY::instructionLoop(Lambda&& condition) noexcept {
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
					CASE_xNF0(0xC0):
						instruction_00CN(_N);
						break;
					case 0x00E0:
						instruction_00E0();
						break;
					case 0x00EE:
						instruction_00EE();
						break;
					case 0x00FB:
						instruction_00FB();
						break;
					case 0x00FC:
						instruction_00FC();
						break;
					case 0x00FD:
						instruction_00FD();
						break;
					case 0x00FE:
						instruction_00FE();
						break;
					case 0x00FF:
						instruction_00FF();
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
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
				if (_N) [[unlikely]] {
					instructionError(HI, LO);
				} else {
					instruction_5xy0(_X, Y_);
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
				instruction_BXNN(_X, _NNN);
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
					case 0x30:
						instruction_Fx30(_X);
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
					case 0x75:
						instruction_FN75(_X);
						break;
					case 0x85:
						instruction_FN85(_X);
						break;
					[[unlikely]]
					default: instructionError(HI, LO);
				}
				break;
		}
	}
}

void SCHIP_LEGACY::renderAudioData() {
	mixAudioData({
		{ makePulseWave, &mVoices[VOICE::ID_0] },
		{ makePulseWave, &mVoices[VOICE::ID_1] },
		{ makePulseWave, &mVoices[VOICE::ID_2] },
		{ makePulseWave, &mVoices[VOICE::BUZZER] },
	});

	setDisplayBorderColor(sBitColors[!!::accumulate(mAudioTimers)]);
}

void SCHIP_LEGACY::renderVideoData() {
	BVS->displayBuffer.write(mDisplayBuffer, isUsingPixelTrails()
		? [](u32 pixel) noexcept {
			return cPixelOpacity[pixel] | sBitColors[pixel != 0];
		}
		: [](u32 pixel) noexcept {
			return 0xFFu | sBitColors[pixel >> 3];
		}
	);

	std::for_each(EXEC_POLICY(unseq)
		mDisplayBuffer.begin(),
		mDisplayBuffer.end(),
		[](auto& pixel) noexcept
			{ ::assign_cast(pixel, (pixel & 0x8) | (pixel >> 1)); }
	);
}

void SCHIP_LEGACY::prepDisplayArea(const Resolution mode) {
	isLargerDisplay(mode != Resolution::LO);

	Quirk.waitVblank = !isLargerDisplay();
	mTargetCPF = isLargerDisplay() ? cInstSpeedLo : cInstSpeedHi;
};

/*==================================================================*/

void SCHIP_LEGACY::scrollDisplayDN(s32 N) {
	mDisplayBuffer.shift(0, +N);
}
void SCHIP_LEGACY::scrollDisplayLT() {
	mDisplayBuffer.shift(-4, 0);
}
void SCHIP_LEGACY::scrollDisplayRT() {
	mDisplayBuffer.shift(+4, 0);
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void SCHIP_LEGACY::instruction_00CN(s32 N) noexcept {
		scrollDisplayDN(N);
	}
	void SCHIP_LEGACY::instruction_00E0() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		mDisplayBuffer.initialize();
	}
	void SCHIP_LEGACY::instruction_00EE() noexcept {
		mCurrentPC = mStackBank[--mStackTop & 0xF];
	}
	void SCHIP_LEGACY::instruction_00FB() noexcept {
		scrollDisplayRT();
	}
	void SCHIP_LEGACY::instruction_00FC() noexcept {
		scrollDisplayLT();
	}
	void SCHIP_LEGACY::instruction_00FD() noexcept {
		triggerInterrupt(Interrupt::SOUND);
	}
	void SCHIP_LEGACY::instruction_00FE() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		prepDisplayArea(Resolution::LO);
	}
	void SCHIP_LEGACY::instruction_00FF() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		prepDisplayArea(Resolution::HI);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void SCHIP_LEGACY::instruction_1NNN(s32 NNN) noexcept {
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void SCHIP_LEGACY::instruction_2NNN(s32 NNN) noexcept {
		::assign_cast(mStackBank[mStackTop++ & 0xF], mCurrentPC);
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void SCHIP_LEGACY::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] == NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void SCHIP_LEGACY::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] != NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void SCHIP_LEGACY::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] == mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void SCHIP_LEGACY::instruction_6xNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void SCHIP_LEGACY::instruction_7xNN(s32 X, s32 NN) noexcept {
		::assign_cast_add(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void SCHIP_LEGACY::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum{ mRegisterV[X] + mRegisterV[Y] };
		::assign_cast(mRegisterV[X], sum);
		::assign_cast(mRegisterV[0xF], sum >> 8);
	}
	void SCHIP_LEGACY::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[X] >= mRegisterV[Y] };
		::assign_cast(mRegisterV[X], mRegisterV[X] - mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void SCHIP_LEGACY::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[Y] >= mRegisterV[X] };
		::assign_cast(mRegisterV[X], mRegisterV[Y] - mRegisterV[X]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void SCHIP_LEGACY::instruction_8xy6(s32 X, s32  ) noexcept {
		const bool lsb{ (mRegisterV[X] & 1) == 1 };
		::assign_cast(mRegisterV[X], mRegisterV[X] >> 1);
		::assign_cast(mRegisterV[0xF], lsb);
	}
	void SCHIP_LEGACY::instruction_8xyE(s32 X, s32  ) noexcept {
		const bool msb{ (mRegisterV[X] >> 7) == 1 };
		::assign_cast(mRegisterV[X], mRegisterV[X] << 1);
		::assign_cast(mRegisterV[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void SCHIP_LEGACY::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] != mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void SCHIP_LEGACY::instruction_ANNN(s32 NNN) noexcept {
		setIndexRegister(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void SCHIP_LEGACY::instruction_BXNN(s32 X, s32 NNN) noexcept {
		performProgJump(NNN + mRegisterV[X]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void SCHIP_LEGACY::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	bool SCHIP_LEGACY::drawSingleBytes(
		s32 originX, s32 originY,
		s32 WIDTH,   s32 DATA
	) noexcept {
		if (!DATA) { return false; }
		bool collided{ false };

		for (auto B{ 0 }; B < WIDTH; ++B) {
			const auto offsetX{ originX + B };

			if (DATA >> (WIDTH - 1 - B) & 0x1) {
				auto& pixel{ mDisplayBuffer(offsetX, originY) };
				if (!((pixel ^= 0x8) & 0x8)) { collided = true; }
			}
			if (offsetX == cScreenSizeX - 1) { return collided; }
		}
		return collided;
	}

	bool SCHIP_LEGACY::drawDoubleBytes(
		s32 originX, s32 originY,
		s32 WIDTH,   s32 DATA
	) noexcept {
		if (!DATA) { return false; }
		bool collided{ false };

		for (auto B{ 0 }; B < WIDTH; ++B) {
			const auto offsetX{ originX + B };

			auto& pixelHI{ mDisplayBuffer(offsetX, originY + 0) };
			auto& pixelLO{ mDisplayBuffer(offsetX, originY + 1) };

			if (DATA >> (WIDTH - 1 - B) & 0x1) {
				if (pixelHI & 0x8) { collided = true; }
				pixelLO = pixelHI ^= 0x8;
			} else {
				pixelLO = pixelHI;
			}
			if (offsetX == cScreenSizeX - 1) { return collided; }
		}
		return collided;
	}

	void SCHIP_LEGACY::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		if (Quirk.waitVblank) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }

		if (isLargerDisplay()) {
			const auto offsetX{ 8 - (mRegisterV[X] & 7) };
			const auto originX{ mRegisterV[X] & 0x78 };
			const auto originY{ mRegisterV[Y] & 0x3F };

			auto collisions{ 0 };

			if (N == 0) {
				for (auto rowN{ 0 }; rowN < 16; ++rowN) {
					const auto offsetY{ originY + rowN };

					collisions += drawSingleBytes(
						originX, offsetY, offsetX ? 24 : 16,
						(readMemoryI(2 * rowN + 0)  << 8 | \
						 readMemoryI(2 * rowN + 1)) << offsetX
					);
					if (offsetY == cScreenSizeY - 1) { break; }
				}
			} else {
				for (auto rowN{ 0 }; rowN < N; ++rowN) {
					const auto offsetY{ originY + rowN };

					collisions += drawSingleBytes(
						originX, offsetY, offsetX ? 16 : 8,
						readMemoryI(rowN) << offsetX
					);
					if (offsetY == cScreenSizeY - 1) { break; }
				}
			}
			::assign_cast(mRegisterV[0xF], collisions);
		}
		else {
			const auto offsetX{ 16 - 2 * (mRegisterV[X] & 0x07) };
			const auto originX{ mRegisterV[X] * 2 & 0x70 };
			const auto originY{ mRegisterV[Y] * 2 & 0x3F };
			const auto lengthN{ N == 0 ? 16 : N };

			auto collisions{ 0 };

			for (auto rowN{ 0 }; rowN < lengthN; ++rowN) {
				const auto offsetY{ originY + rowN * 2 };

				collisions += drawDoubleBytes(originX, offsetY, 0x20,
					ez::bitDup8(readMemoryI(rowN)) << offsetX);

				if (offsetY == cScreenSizeY - 2) { break; }
			}
			::assign_cast(mRegisterV[0xF], collisions != 0);
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void SCHIP_LEGACY::instruction_Ex9E(s32 X) noexcept {
		if (keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}
	void SCHIP_LEGACY::instruction_ExA1(s32 X) noexcept {
		if (!keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void SCHIP_LEGACY::instruction_Fx07(s32 X) noexcept {
		::assign_cast(mRegisterV[X], mDelayTimer);
	}
	void SCHIP_LEGACY::instruction_Fx0A(s32 X) noexcept {
		triggerInterrupt(Interrupt::INPUT);
		mInputReg = &mRegisterV[X];
	}
	void SCHIP_LEGACY::instruction_Fx15(s32 X) noexcept {
		::assign_cast(mDelayTimer, mRegisterV[X]);
	}
	void SCHIP_LEGACY::instruction_Fx18(s32 X) noexcept {
		startVoice(mRegisterV[X] + (mRegisterV[X] == 1));
	}
	void SCHIP_LEGACY::instruction_Fx1E(s32 X) noexcept {
		incIndexRegister(mRegisterV[X]);
	}
	void SCHIP_LEGACY::instruction_Fx29(s32 X) noexcept {
		setIndexRegister((mRegisterV[X] & 0xF) * 5 + cSmallFontOffset);
	}
	void SCHIP_LEGACY::instruction_Fx30(s32 X) noexcept {
		setIndexRegister((mRegisterV[X] & 0xF) * 10 + cLargeFontOffset);
	}
	void SCHIP_LEGACY::instruction_Fx33(s32 X) noexcept {
		const TriBCD bcd{ mRegisterV[X] };

		writeMemoryI(bcd.digit[2], 0);
		writeMemoryI(bcd.digit[1], 1);
		writeMemoryI(bcd.digit[0], 2);
	}
	void SCHIP_LEGACY::instruction_FN55(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { writeMemoryI(mRegisterV[idx], idx); }
		if (Quirk.idxRegMinus) [[likely]] { incIndexRegister(N); }
	}
	void SCHIP_LEGACY::instruction_FN65(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { mRegisterV[idx] = readMemoryI(idx); }
		if (Quirk.idxRegMinus) [[likely]] { incIndexRegister(N); }
	}
	void SCHIP_LEGACY::instruction_FN75(s32 N) noexcept {
		setPermaRegs(std::min(N, 7) + 1);
	}
	void SCHIP_LEGACY::instruction_FN85(s32 N) noexcept {
		getPermaRegs(std::min(N, 7) + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
