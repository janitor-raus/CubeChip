/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "XOCHIP.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_XOCHIP)

#include "BasicVideoSpec.hpp"

#include "CoreRegistry.hpp"

REGISTER_CORE(XOCHIP, ".xo8")

/*==================================================================*/

void XOCHIP::initializeSystem() noexcept {
	Quirk.wrapSprite = true;

	::fill_n(mMemoryBank, cTotalMemory, cSafezoneOOB, 0xFF);

	copyGameToMemory(mMemoryBank.data() + cGameLoadPos);
	copyFontToMemory(mMemoryBank.data(), 80);
	copyColorsToCore(mBitColors.data());

	mDisplay.set(cScreenSizeX, cScreenSizeY);
	setViewportSizes(true, cScreenSizeX, cScreenSizeY, cResSizeMult, 2);
	setBaseSystemFramerate(cRefreshRate);

	setPatternPitch(64);

	mVoices[VOICE::UNIQUE].userdata = &mAudioTimers[VOICE::UNIQUE];
	mVoices[VOICE::BUZZER].userdata = &mAudioTimers[VOICE::BUZZER];

	mCurrentPC = cStartOffset;
	mTargetCPF = cInstSpeedLo;
}

void XOCHIP::handleCycleLoop() noexcept
	{ LOOP_DISPATCH(instructionLoop); }

template <typename Lambda>
void XOCHIP::instructionLoop(Lambda&& condition) noexcept {
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
					CASE_xNF0(0xD0):
						instruction_00DN(_N);
						break;
					case 0xE0:
						instruction_00E0();
						break;
					case 0xEE:
						instruction_00EE();
						break;
					case 0xFB:
						instruction_00FB();
						break;
					case 0xFC:
						instruction_00FC();
						break;
					case 0xFD:
						instruction_00FD();
						break;
					case 0xFE:
						instruction_00FE();
						break;
					case 0xFF:
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
				switch (LO) {
					CASE_xFN(0x00):
						instruction_5xy0(_X, Y_);
						break;
					CASE_xFN(0x02):
						instruction_5xy2(_X, Y_);
						break;
					CASE_xFN(0x03):
						instruction_5xy3(_X, Y_);
						break;
					CASE_xFN(0x04):
						instruction_5xy4(_X, Y_);
						break;
					[[unlikely]]
					default:
						instructionError(HI, LO);
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
			case 0xF0:
				/**/ if (LO == 0x00) {
					instruction_F000();
					break;
				}
				else if (LO == 0x02) {
					instruction_F002();
					break;
				}
				[[fallthrough]];
			CASE_xNF0(0xF0):
				switch (LO) {
					case 0x01:
						instruction_FN01(_X);
						break;
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
					case 0x3A:
						instruction_Fx3A(_X);
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

void XOCHIP::renderAudioData() {
	mixAudioData({
		{ makePatternWave, &mVoices[VOICE::UNIQUE] },
		{ makePulseWave,   &mVoices[VOICE::BUZZER] },
	});

	setDisplayBorderColor(mBitColors[!!mAudioTimers[VOICE::BUZZER].get()]);
}

void XOCHIP::renderVideoData() {
	std::vector<u8> textureBuffer(mDisplay.pixels());

	std::for_each(EXEC_POLICY(unseq)
		textureBuffer.begin(),
		textureBuffer.end(),
		[&](auto& pixel) noexcept {
			const auto idx{ &pixel - textureBuffer.data() };
			::assign_cast(pixel,
				mDisplayBuffer[3](idx) << 3 |
				mDisplayBuffer[2](idx) << 2 |
				mDisplayBuffer[1](idx) << 1 |
				mDisplayBuffer[0](idx)
			);
		}
	);

	BVS->displayBuffer.write(textureBuffer,
		[pBitColors = mBitColors.data()](u32 pixel) noexcept {
			return u32(0xFFu | pBitColors[pixel]);
		}
	);

	setViewportSizes(isResolutionChanged(false), mDisplay.W, mDisplay.H,
		isLargerDisplay() ? cResSizeMult / 2 : cResSizeMult, 2);
}

void XOCHIP::prepDisplayArea(const Resolution mode) {
	const bool wasLargerDisplay{ isLargerDisplay(mode != Resolution::LO) };
	isResolutionChanged(wasLargerDisplay != isLargerDisplay());

	const auto W{ isLargerDisplay() ? cScreenSizeX * 2 : cScreenSizeX };
	const auto H{ isLargerDisplay() ? cScreenSizeY * 2 : cScreenSizeY };

	mDisplay.set(W, H);

	mDisplayBuffer[0].resizeClean(W, H);
	mDisplayBuffer[1].resizeClean(W, H);
	mDisplayBuffer[2].resizeClean(W, H);
	mDisplayBuffer[3].resizeClean(W, H);
};

void XOCHIP::setColorBit332(s32 bit, s32 index) noexcept {
	mBitColors[bit & 0xF] = sColorPalette[index];
}

void XOCHIP::setPatternPitch(s32 pitch) noexcept {
	if (auto* stream{ mAudioDevice.at(STREAM::MAIN) }) {
		mVoices[VOICE::UNIQUE].setStep(std::bit_cast<f32>
			(sPitchFreqLUT[pitch]) / stream->getFreq() * getFramerateMultiplier());
	}
}

void XOCHIP::makePatternWave(f32* data, u32 size, Voice* voice, Stream*) noexcept {
	if (!voice || !voice->userdata) [[unlikely]] { return; }
	auto* timer{ static_cast<AudioTimer*>(voice->userdata) };

	for (auto i{ 0u }; i < size; ++i) {
		if (const auto gain{ voice->getLevel(i, *timer) }) {
			const auto bitStep{ s32(voice->peekPhase(i) * 128.0f) };
			const auto bitMask{ 1 << (0x7 ^ (bitStep & 0x7)) };
			::assign_cast_add(data[i], \
				(mPattern[bitStep >> 3] & bitMask) ? gain : -gain);
		} else break;
	}
	voice->stepPhase(size);
}

/*==================================================================*/

void XOCHIP::skipInstruction() noexcept {
	mCurrentPC += NNNN() == 0xF000 ? 4 : 2;
}

void XOCHIP::scrollDisplayUP(s32 N) {
	if (mPlanarMask & 0x1) { mDisplayBuffer[0].shift(0, -N); }
	if (mPlanarMask & 0x2) { mDisplayBuffer[1].shift(0, -N); }
	if (mPlanarMask & 0x4) { mDisplayBuffer[2].shift(0, -N); }
	if (mPlanarMask & 0x8) { mDisplayBuffer[3].shift(0, -N); }
}
void XOCHIP::scrollDisplayDN(s32 N) {
	if (mPlanarMask & 0x1) { mDisplayBuffer[0].shift(0, +N); }
	if (mPlanarMask & 0x2) { mDisplayBuffer[1].shift(0, +N); }
	if (mPlanarMask & 0x4) { mDisplayBuffer[2].shift(0, +N); }
	if (mPlanarMask & 0x8) { mDisplayBuffer[3].shift(0, +N); }
}
void XOCHIP::scrollDisplayLT() {
	if (mPlanarMask & 0x1) { mDisplayBuffer[0].shift(-4, 0); }
	if (mPlanarMask & 0x2) { mDisplayBuffer[1].shift(-4, 0); }
	if (mPlanarMask & 0x4) { mDisplayBuffer[2].shift(-4, 0); }
	if (mPlanarMask & 0x8) { mDisplayBuffer[3].shift(-4, 0); }
}
void XOCHIP::scrollDisplayRT() {
	if (mPlanarMask & 0x1) { mDisplayBuffer[0].shift(+4, 0); }
	if (mPlanarMask & 0x2) { mDisplayBuffer[1].shift(+4, 0); }
	if (mPlanarMask & 0x4) { mDisplayBuffer[2].shift(+4, 0); }
	if (mPlanarMask & 0x8) { mDisplayBuffer[3].shift(+4, 0); }
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void XOCHIP::instruction_00CN(s32 N) noexcept {
		if (Quirk.waitScroll) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
		if (N) { scrollDisplayDN(N); }
	}
	void XOCHIP::instruction_00DN(s32 N) noexcept {
		if (Quirk.waitScroll) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
		if (N) { scrollDisplayUP(N); }
	}
	void XOCHIP::instruction_00E0() noexcept {
		if (mPlanarMask & 0x1) { mDisplayBuffer[0].initialize(); }
		if (mPlanarMask & 0x2) { mDisplayBuffer[1].initialize(); }
		if (mPlanarMask & 0x4) { mDisplayBuffer[2].initialize(); }
		if (mPlanarMask & 0x8) { mDisplayBuffer[3].initialize(); }
	}
	void XOCHIP::instruction_00EE() noexcept {
		mCurrentPC = mStackBank[--mStackTop & 0xF];
	}
	void XOCHIP::instruction_00FB() noexcept {
		if (Quirk.waitScroll) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
		scrollDisplayRT();
	}
	void XOCHIP::instruction_00FC() noexcept {
		if (Quirk.waitScroll) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }
		scrollDisplayLT();
	}
	void XOCHIP::instruction_00FD() noexcept {
		triggerInterrupt(Interrupt::SOUND);
	}
	void XOCHIP::instruction_00FE() noexcept {
		prepDisplayArea(Resolution::LO);
	}
	void XOCHIP::instruction_00FF() noexcept {
		prepDisplayArea(Resolution::HI);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void XOCHIP::instruction_1NNN(s32 NNN) noexcept {
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void XOCHIP::instruction_2NNN(s32 NNN) noexcept {
		mStackBank[mStackTop++ & 0xF] = mCurrentPC;
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void XOCHIP::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] == NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void XOCHIP::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] != NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void XOCHIP::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] == mRegisterV[Y]) { skipInstruction(); }
	}
	void XOCHIP::instruction_5xy2(s32 X, s32 Y) noexcept {
		const auto dist{ std::abs(X - Y) + 1 };
		const auto flip{ X < Y ? 1 : -1 };
		for (auto Z{ 0 }; Z < dist; ++Z)
			{ writeMemoryI(mRegisterV[X + Z * flip], Z); }
	}
	void XOCHIP::instruction_5xy3(s32 X, s32 Y) noexcept {
		const auto dist{ std::abs(X - Y) + 1 };
		const auto flip{ X < Y ? 1 : -1 };
		for (auto Z{ 0 }; Z < dist; ++Z)
			{ mRegisterV[X + Z * flip] = readMemoryI(Z); }
	}
	void XOCHIP::instruction_5xy4(s32 X, s32 Y) noexcept {
		const auto dist{ std::abs(X - Y) + 1 };
		const auto flip{ X < Y ? 1 : -1 };
		for (auto Z{ 0 }; Z < dist; ++Z)
			{ setColorBit332(X + Z * flip, readMemoryI(Z)); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void XOCHIP::instruction_6xNN(s32 X, s32 NN) noexcept {
		mRegisterV[X] = static_cast<u8>(NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void XOCHIP::instruction_7xNN(s32 X, s32 NN) noexcept {
		mRegisterV[X] += static_cast<u8>(NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void XOCHIP::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(mRegisterV[X], mRegisterV[Y]);
	}
	void XOCHIP::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(mRegisterV[X], mRegisterV[Y]);
	}
	void XOCHIP::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(mRegisterV[X], mRegisterV[Y]);
	}
	void XOCHIP::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(mRegisterV[X], mRegisterV[Y]);
	}
	void XOCHIP::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum{ mRegisterV[X] + mRegisterV[Y] };
		::assign_cast(mRegisterV[X], sum);
		::assign_cast(mRegisterV[0xF], sum >> 8);
	}
	void XOCHIP::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[X] >= mRegisterV[Y] };
		::assign_cast(mRegisterV[X], mRegisterV[X] - mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void XOCHIP::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[Y] >= mRegisterV[X] };
		::assign_cast(mRegisterV[X], mRegisterV[Y] - mRegisterV[X]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void XOCHIP::instruction_8xy6(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { mRegisterV[X] = mRegisterV[Y]; }
		const bool lsb{ (mRegisterV[X] & 1) == 1 };
		::assign_cast(mRegisterV[X], mRegisterV[X] >> 1);
		::assign_cast(mRegisterV[0xF], lsb);
	}
	void XOCHIP::instruction_8xyE(s32 X, s32 Y) noexcept {
		if (!Quirk.shiftVX) { mRegisterV[X] = mRegisterV[Y]; }
		const bool msb{ (mRegisterV[X] >> 7) == 1 };
		::assign_cast(mRegisterV[X], mRegisterV[X] << 1);
		::assign_cast(mRegisterV[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void XOCHIP::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] != mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void XOCHIP::instruction_ANNN(s32 NNN) noexcept {
		setIndexRegister(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void XOCHIP::instruction_BNNN(s32 NNN) noexcept {
		performProgJump(NNN + mRegisterV[0]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void XOCHIP::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void XOCHIP::drawByte(s32 X, s32 Y, s32 P, u32 DATA) noexcept {
		switch (DATA) {
			[[unlikely]]
			case 0b00000000:
				return;

			[[unlikely]]
			case 0b10000000:
				if (Quirk.wrapSprite) { X &= (mDisplay.W - 1); }
				if (X < mDisplay.W) {
					if (!((mDisplayBuffer[P](X, Y) ^= 1) & 1))
						{ mRegisterV[0xF] = 1; }
				}
				return;

			[[likely]]
			default:
				if (Quirk.wrapSprite) { X &= (mDisplay.W - 1); }
				else if (X >= mDisplay.W) { return; }

				for (auto B{ 0 }; B < 8; ++B, ++X &= (mDisplay.W - 1)) {
					if (DATA & 0x80 >> B) {
						if (!((mDisplayBuffer[P](X, Y) ^= 1) & 1))
							{ mRegisterV[0xF] = 1; }
					}
					if (!Quirk.wrapSprite && X == (mDisplay.W - 1)) { return; }
				}
				return;
		}
	}

	template <std::size_t P>
	void XOCHIP::drawSingleRow(s32 X, s32 Y) noexcept {
		drawByte(X, Y, P, readMemoryI(sPlaneMult[P][mPlanarMask]));
	}

	template <std::size_t P>
	void XOCHIP::drawDoubleRow(s32 X, s32 Y) noexcept {
		const auto I{ sPlaneMult[P][mPlanarMask] * 32 };

		for (auto H{ 0 }; H < 16; ++H) {
			drawByte(X + 0, Y, P, readMemoryI(I + H * 2 + 0));
			drawByte(X + 8, Y, P, readMemoryI(I + H * 2 + 1));

			if (!Quirk.wrapSprite && Y == (mDisplay.H - 1)) { break; }
			else { ++Y &= (mDisplay.H - 1); }
		}
	}

	template <std::size_t P>
	void XOCHIP::drawMultiRow(s32 X, s32 Y, s32 N) noexcept {
		const auto I{ sPlaneMult[P][mPlanarMask] * N };

		for (auto H{ 0 }; H < N; ++H) {
			drawByte(X, Y, P, readMemoryI(I + H));

			if (!Quirk.wrapSprite && Y == (mDisplay.H - 1)) { break; }
			else { ++Y &= (mDisplay.H - 1); }
		}
	}

	void XOCHIP::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		const auto pX{ mRegisterV[X] & (mDisplay.W - 1) };
		const auto pY{ mRegisterV[Y] & (mDisplay.H - 1) };

		mRegisterV[0xF] = 0;

		switch (N) {
			case 0:
				if (mPlanarMask & P0M) { drawDoubleRow<P0>(pX, pY); }
				if (mPlanarMask & P1M) { drawDoubleRow<P1>(pX, pY); }
				if (mPlanarMask & P2M) { drawDoubleRow<P2>(pX, pY); }
				if (mPlanarMask & P3M) { drawDoubleRow<P3>(pX, pY); }
				break;

			case 1:
				if (mPlanarMask & P0M) { drawSingleRow<P0>(pX, pY); }
				if (mPlanarMask & P1M) { drawSingleRow<P1>(pX, pY); }
				if (mPlanarMask & P2M) { drawSingleRow<P2>(pX, pY); }
				if (mPlanarMask & P3M) { drawSingleRow<P3>(pX, pY); }
				break;

			default:
				if (mPlanarMask & P0M) { drawMultiRow<P0>(pX, pY, N); }
				if (mPlanarMask & P1M) { drawMultiRow<P1>(pX, pY, N); }
				if (mPlanarMask & P2M) { drawMultiRow<P2>(pX, pY, N); }
				if (mPlanarMask & P3M) { drawMultiRow<P3>(pX, pY, N); }
				break;
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void XOCHIP::instruction_Ex9E(s32 X) noexcept {
		if (keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}
	void XOCHIP::instruction_ExA1(s32 X) noexcept {
		if (!keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void XOCHIP::instruction_F000() noexcept {
		setIndexRegister(NNNN());
		::assign_cast_add(mCurrentPC, 2);
	}
	void XOCHIP::instruction_F002() noexcept {
		SUGGEST_VECTORIZABLE_LOOP
		for (auto idx{ 0 }; idx < 16; ++idx)
			{ mPattern[idx] = readMemoryI(idx); }
	}
	void XOCHIP::instruction_FN01(s32 N) noexcept {
		mPlanarMask = N;
	}
	void XOCHIP::instruction_Fx07(s32 X) noexcept {
		::assign_cast(mRegisterV[X], mDelayTimer);
	}
	void XOCHIP::instruction_Fx0A(s32 X) noexcept {
		triggerInterrupt(Interrupt::INPUT);
		mInputReg = &mRegisterV[X];
	}
	void XOCHIP::instruction_Fx15(s32 X) noexcept {
		mDelayTimer = mRegisterV[X];
	}
	void XOCHIP::instruction_Fx18(s32 X) noexcept {
		mAudioTimers[VOICE::UNIQUE].set(mRegisterV[X] + (mRegisterV[X] == 1));
	}
	void XOCHIP::instruction_Fx1E(s32 X) noexcept {
		incIndexRegister(mRegisterV[X]);
	}
	void XOCHIP::instruction_Fx29(s32 X) noexcept {
		setIndexRegister((mRegisterV[X] & 0xF) * 5 + cSmallFontOffset);
	}
	void XOCHIP::instruction_Fx30(s32 X) noexcept {
		setIndexRegister((mRegisterV[X] & 0xF) * 10 + cLargeFontOffset);
	}
	void XOCHIP::instruction_Fx33(s32 X) noexcept {
		const TriBCD bcd{ mRegisterV[X] };

		writeMemoryI(bcd.digit[2], 0);
		writeMemoryI(bcd.digit[1], 1);
		writeMemoryI(bcd.digit[0], 2);
	}
	void XOCHIP::instruction_Fx3A(s32 X) noexcept {
		setPatternPitch(mRegisterV[X]);
	}
	void XOCHIP::instruction_FN55(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { writeMemoryI(mRegisterV[idx], idx); }
		if (!Quirk.idxRegNoInc) [[likely]] { incIndexRegister(N + 1); }
	}
	void XOCHIP::instruction_FN65(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx) { mRegisterV[idx] = readMemoryI(idx); }
		if (!Quirk.idxRegNoInc) [[likely]] { incIndexRegister(N + 1); }
	}
	void XOCHIP::instruction_FN75(s32 N) noexcept {
		setPermaRegs(N + 1);
	}
	void XOCHIP::instruction_FN85(s32 N) noexcept {
		getPermaRegs(N + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/


#endif
