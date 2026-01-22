/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "SCHIP_MODERN.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_SCHIP_MODERN)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(SCHIP_MODERN, ".sc8")

/*==================================================================*/

void SCHIP_MODERN::initializeSystem() noexcept {
	copyGameToMemory(mMemoryBank.data() + cGameLoadPos);
	copyFontToMemory(mMemoryBank.data(), 240);

	mDisplay.set(cScreenSizeX, cScreenSizeY);
	set_base_system_framerate(cRefreshRate);

	mVoices[VOICE::ID_0].userdata = &mAudioTimers[VOICE::ID_0];
	mVoices[VOICE::ID_1].userdata = &mAudioTimers[VOICE::ID_1];
	mVoices[VOICE::ID_2].userdata = &mAudioTimers[VOICE::ID_2];
	mVoices[VOICE::ID_3].userdata = &mAudioTimers[VOICE::ID_3];

	mCurrentPC = cStartOffset;
	mTargetCPF = cInstSpeedLo;

	prepDisplayArea(Resolution::LO);

	mDisplayDevice.metadata_staging
		.set_viewport(64, 32)
		.set_inner_margin(4)
		.set_texture_tint(sBitColors[0])
		.enabled = true;
}

void SCHIP_MODERN::handleCycleLoop() noexcept
	{ LOOP_DISPATCH(instructionLoop); }

template <typename Lambda>
void SCHIP_MODERN::instructionLoop(Lambda&& condition) noexcept {
	for (mCycleCount = 0; condition(); ++mCycleCount) {
		const auto HI = mMemoryBank[mCurrentPC++];
		const auto LO = mMemoryBank[mCurrentPC++];

		#define _NNN ((HI << 8 | LO) & 0xFFF)
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
				instruction_BNNN(_NNN);
				break;
			CASE_xNF(0xC0):
				instruction_CxNN(_X, LO);
				break;
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

void SCHIP_MODERN::renderAudioData() {
	mixAudioData({
		{ makePulseWave, &mVoices[VOICE::ID_0] },
		{ makePulseWave, &mVoices[VOICE::ID_1] },
		{ makePulseWave, &mVoices[VOICE::ID_2] },
		{ makePulseWave, &mVoices[VOICE::BUZZER] },
	});

	mDisplayDevice.metadata_staging.set_border_color_if(
		!!::accumulate(mAudioTimers), sBitColors[1]);
}

void SCHIP_MODERN::renderVideoData() {
	mDisplayDevice.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = ++mDisplayDevice.metadata_staging;
		frame.copy_from(mDisplayBuffer, isUsingPixelTrails()
			? [](u32 pixel) noexcept { return RGBA::premul(sBitColors[pixel != 0], cBitWeight[pixel]); }
			: [](u32 pixel) noexcept { return sBitColors[pixel >> 3]; }
		);
	});

	std::for_each(EXEC_POLICY(unseq)
		mDisplayBuffer.begin(),
		mDisplayBuffer.end(),
		[](auto& pixel) noexcept
			{ ::assign_cast(pixel, (pixel & 0x8) | (pixel >> 1)); }
	);
}

void SCHIP_MODERN::prepDisplayArea(const Resolution mode) {
	isLargerDisplay(mode != Resolution::LO);

	const auto W = isLargerDisplay() ? cDisplayW : cDisplayW / 2;
	const auto H = isLargerDisplay() ? cDisplayH : cDisplayH / 2;

	mDisplayDevice.metadata_staging.set_viewport(W, H)
		.set_texture_zoom(isLargerDisplay() ? 4 : 8);

	mDisplay.set(W, H);

	mDisplayBuffer.resizeClean(W, H);
};

/*==================================================================*/

void SCHIP_MODERN::scrollDisplayDN(s32 N) {
	mDisplayBuffer.shift(0, +N);
}
void SCHIP_MODERN::scrollDisplayLT() {
	mDisplayBuffer.shift(-4, 0);
}
void SCHIP_MODERN::scrollDisplayRT() {
	mDisplayBuffer.shift(+4, 0);
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void SCHIP_MODERN::instruction_00CN(s32 N) noexcept {
		if (N) { scrollDisplayDN(N); }
		if (Quirk.waitScroll) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00E0() noexcept {
		mDisplayBuffer.initialize();
		if (Quirk.waitVblank) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00EE() noexcept {
		mCurrentPC = mStackBank[--mStackTop & 0xF];
	}
	void SCHIP_MODERN::instruction_00FB() noexcept {
		scrollDisplayRT();
		if (Quirk.waitScroll) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00FC() noexcept {
		scrollDisplayLT();
		if (Quirk.waitScroll) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00FD() noexcept {
		triggerInterrupt(Interrupt::SOUND);
	}
	void SCHIP_MODERN::instruction_00FE() noexcept {
		prepDisplayArea(Resolution::LO);
		if (Quirk.waitVblank) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00FF() noexcept {
		prepDisplayArea(Resolution::HI);
		if (Quirk.waitVblank) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void SCHIP_MODERN::instruction_1NNN(s32 NNN) noexcept {
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void SCHIP_MODERN::instruction_2NNN(s32 NNN) noexcept {
		mStackBank[mStackTop++ & 0xF] = mCurrentPC;
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void SCHIP_MODERN::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] == NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void SCHIP_MODERN::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] != NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void SCHIP_MODERN::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] == mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void SCHIP_MODERN::instruction_6xNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void SCHIP_MODERN::instruction_7xNN(s32 X, s32 NN) noexcept {
		::assign_cast_add(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void SCHIP_MODERN::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_MODERN::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_MODERN::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_MODERN::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(mRegisterV[X], mRegisterV[Y]);
	}
	void SCHIP_MODERN::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum = mRegisterV[X] + mRegisterV[Y];
		::assign_cast(mRegisterV[X], sum);
		::assign_cast(mRegisterV[0xF], sum >> 8);
	}
	void SCHIP_MODERN::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow = mRegisterV[X] >= mRegisterV[Y];
		::assign_cast_sub(mRegisterV[X], mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void SCHIP_MODERN::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow = mRegisterV[Y] >= mRegisterV[X];
		::assign_cast_rsub(mRegisterV[X], mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void SCHIP_MODERN::instruction_8xy6(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { mRegisterV[X] = mRegisterV[Y]; }
		const bool lsb = (mRegisterV[X] & 1) == 1;
		::assign_cast_shr(mRegisterV[X], 1);
		::assign_cast(mRegisterV[0xF], lsb);
	}
	void SCHIP_MODERN::instruction_8xyE(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { mRegisterV[X] = mRegisterV[Y]; }
		const bool msb = (mRegisterV[X] >> 7) == 1;
		::assign_cast_shl(mRegisterV[X], 1);
		::assign_cast(mRegisterV[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void SCHIP_MODERN::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] != mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void SCHIP_MODERN::instruction_ANNN(s32 NNN) noexcept {
		::assign_cast(mRegisterI, NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void SCHIP_MODERN::instruction_BNNN(s32 NNN) noexcept {
		performProgJump(NNN + mRegisterV[0]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void SCHIP_MODERN::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void SCHIP_MODERN::drawByte(s32 X, s32 Y, u32 DATA) noexcept {
		switch (DATA) {
			[[unlikely]]
			case 0b00000000:
				return;

			[[unlikely]]
			case 0b10000000:
				if (Quirk.wrapSprite) { X &= (mDisplay.W - 1); }
				if (X < mDisplay.W) {
					if (!((mDisplayBuffer(X, Y) ^= 0x8) & 0x8))
						{ mRegisterV[0xF] = 1; }
				}
				return;

			[[likely]]
			default:
				if (Quirk.wrapSprite) { X &= (mDisplay.W - 1); }
				else if (X >= mDisplay.W) { return; }

				for (auto B = 0; B < 8; ++B, ++X &= (mDisplay.W - 1)) {
					if (DATA & 0x80 >> B) {
						if (!((mDisplayBuffer(X, Y) ^= 0x8) & 0x8))
							{ mRegisterV[0xF] = 1; }
					}
					if (!Quirk.wrapSprite && X == (mDisplay.W - 1)) { return; }
				}
				return;
		}
	}

	void SCHIP_MODERN::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		const auto pX = mRegisterV[X] & (mDisplay.W - 1);
		const auto pY = mRegisterV[Y] & (mDisplay.H - 1);

		mRegisterV[0xF] = 0;

		switch (N) {
			[[unlikely]]
			case 1:
				drawByte(pX, pY, mMemoryBank[mRegisterI]);
				break;

			[[unlikely]]
			case 0:
				for (auto tN = 0, tY = pY; tN < 32;)
				{
					drawByte(pX + 0, tY, mMemoryBank[mRegisterI + tN + 0]);
					drawByte(pX + 8, tY, mMemoryBank[mRegisterI + tN + 1]);
					if (!Quirk.wrapSprite && tY == (mDisplay.H - 1)) { break; }
					else { tN += 2; ++tY &= (mDisplay.H - 1); }
				}
				break;

			[[likely]]
			default:
				for (auto tN = 0, tY = pY; tN < N;)
				{
					drawByte(pX, tY, mMemoryBank[mRegisterI + tN]);
					if (!Quirk.wrapSprite && tY == (mDisplay.H - 1)) { break; }
					else { tN += 1; ++tY &= (mDisplay.H - 1); }
				}
				break;
		}

		if (Quirk.waitVblank) [[unlikely]] { triggerInterrupt(Interrupt::FRAME); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void SCHIP_MODERN::instruction_Ex9E(s32 X) noexcept {
		if (keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}
	void SCHIP_MODERN::instruction_ExA1(s32 X) noexcept {
		if (!keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void SCHIP_MODERN::instruction_Fx07(s32 X) noexcept {
		::assign_cast(mRegisterV[X], mDelayTimer);
	}
	void SCHIP_MODERN::instruction_Fx0A(s32 X) noexcept {
		mInputReg = &mRegisterV[X];
		triggerInterrupt(Interrupt::INPUT);
	}
	void SCHIP_MODERN::instruction_Fx15(s32 X) noexcept {
		::assign_cast(mDelayTimer, mRegisterV[X]);
	}
	void SCHIP_MODERN::instruction_Fx18(s32 X) noexcept {
		startVoice(mRegisterV[X] + (mRegisterV[X] == 1));
	}
	void SCHIP_MODERN::instruction_Fx1E(s32 X) noexcept {
		::assign_cast_add(mRegisterI, mRegisterV[X]);
	}
	void SCHIP_MODERN::instruction_Fx29(s32 X) noexcept {
		::assign_cast(mRegisterI, (mRegisterV[X] & 0xF) * 5 + cSmallFontOffset);
	}
	void SCHIP_MODERN::instruction_Fx30(s32 X) noexcept {
		::assign_cast(mRegisterI, (mRegisterV[X] & 0xF) * 10 + cLargeFontOffset);
	}
	void SCHIP_MODERN::instruction_Fx33(s32 X) noexcept {
		const TriBCD bcd{ mRegisterV[X] };

		mMemoryBank[mRegisterI + 0] = bcd.digit[2];
		mMemoryBank[mRegisterI + 1] = bcd.digit[1];
		mMemoryBank[mRegisterI + 2] = bcd.digit[0];
	}
	void SCHIP_MODERN::instruction_FN55(s32 N) noexcept {
		for (auto idx = 0; idx <= N; ++idx) { mMemoryBank[mRegisterI + idx] = mRegisterV[idx]; }
		if (!Quirk.idxRegNoInc) [[likely]] { ::assign_cast_add(mRegisterI, N + 1); }
	}
	void SCHIP_MODERN::instruction_FN65(s32 N) noexcept {
		for (auto idx = 0; idx <= N; ++idx) { mRegisterV[idx] = mMemoryBank[mRegisterI + idx]; }
		if (!Quirk.idxRegNoInc) [[likely]] { ::assign_cast_add(mRegisterI, N + 1); }
	}
	void SCHIP_MODERN::instruction_FN75(s32 N) noexcept {
		setPermaRegs(N + 1);
	}
	void SCHIP_MODERN::instruction_FN85(s32 N) noexcept {
		getPermaRegs(N + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
