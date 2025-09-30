/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "MEGACHIP.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_MEGACHIP)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(MEGACHIP, ".mc8")

/*==================================================================*/

void MEGACHIP::initializeSystem() noexcept {
	copyGameToMemory(mMemoryBank.data() + cGameLoadPos);
	copyFontToMemory(mMemoryBank.data(), 180);

	setViewportSizes(true, cScreenMegaX, cScreenMegaY, cResSizeMult, 2);
	setBaseSystemFramerate(cRefreshRate);

	mVoices[VOICE::UNIQUE].userdata = &mAudioTimers[VOICE::UNIQUE];
	mVoices[VOICE::BUZZER].userdata = &mAudioTimers[VOICE::BUZZER];

	mCurrentPC = cStartOffset;

	prepDisplayArea(Resolution::LO);
}

void MEGACHIP::handleCycleLoop() noexcept
	{ LOOP_DISPATCH(instructionLoop); }

template <typename Lambda>
void MEGACHIP::instructionLoop(Lambda&& condition) noexcept {
	for (mCycleCount = 0; condition(); ++mCycleCount) {
		const auto HI{ mMemoryBank[mCurrentPC++] };
		const auto LO{ mMemoryBank[mCurrentPC++] };

		#define _NNN (HI << 8 | LO)
		#define _X (HI & 0xF)
		#define Y_ (LO >> 4)
		#define _N (LO & 0xF)

		switch (HI) {
			CASE_xNF(0x00):
				if (isManualRefresh()) {
					switch (_NNN) {
						case 0x0010:
							instruction_0010();
							break;
						case 0x0700:
							instruction_0700();
							break;
						CASE_xNF(0x0600):
							instruction_060N(_N);
							break;
						CASE_xNF(0x0800):
							instruction_080N(_N);
							break;
						CASE_xNF(0x00B0):
							instruction_00BN(_N);
							break;
						CASE_xNF(0x00C0):
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
						default:
							switch (_X) {
								case 0x01:
									instruction_01NN(LO);
									break;
								case 0x02:
									instruction_02NN(LO);
									break;
								case 0x03:
									instruction_03NN(LO);
									break;
								case 0x04:
									instruction_04NN(LO);
									break;
								case 0x05:
									instruction_05NN(LO);
									break;
								case 0x09:
									instruction_09NN(LO);
									break;
								[[unlikely]]
								default: instructionError(HI, LO);
							}
					}
				}
				else {
					if (_X) [[unlikely]] {
						instructionError(HI, LO);
					} else {
						switch (LO) {
							case 0x11:
								instruction_0011();
								break;
							CASE_xNF0(0xB0):
								instruction_00BN(_N);
								break;
							CASE_xNF0(0xC0):
								instruction_00CN(_N);
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
					}
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

void MEGACHIP::renderAudioData() {
	if (isManualRefresh()) {
		mixAudioData({
			{ makeByteWave,  &mVoices[VOICE::UNIQUE] },
			{ makePulseWave, &mVoices[VOICE::BUZZER] },
		});

		setDisplayBorderColor(sBitColors[!!mAudioTimers[VOICE::BUZZER]]);
	}
	else {
		mixAudioData({
			{ makePulseWave, &mVoices[VOICE::ID_0] },
			{ makePulseWave, &mVoices[VOICE::ID_1] },
			{ makePulseWave, &mVoices[VOICE::ID_2] },
			{ makePulseWave, &mVoices[VOICE::BUZZER] },
		});

		setDisplayBorderColor(sBitColors[!!::accumulate(mAudioTimers)]);
	}
}

void MEGACHIP::renderVideoData() {
	if (!isManualRefresh()) {
		for (auto i{ 0u }; i < mDisplayBuffer.size(); ++i) {
			auto pixel{ mDisplayBuffer[i] };
			auto color{ isUsingPixelTrails()
				? (sBitColors[pixel != 0] | cPixelOpacity[pixel])
				: (sBitColors[pixel >> 3] | 0xFFu) };

			auto x{ (i % cScreenSizeX) * 2 };
			auto y{ (i / cScreenSizeX) * 2 + 32 };

			mBackgroundBuffer(x + 0, y + 0) = color;
			mBackgroundBuffer(x + 1, y + 0) = color;
			mBackgroundBuffer(x + 0, y + 1) = color;
			mBackgroundBuffer(x + 1, y + 1) = color;
		}

		BVS->displayBuffer.write(mBackgroundBuffer);
	}
}

void MEGACHIP::prepDisplayArea(Resolution mode) {
	const bool wasManualRefresh{ isManualRefresh(mode == Resolution::MC) };
	isResolutionChanged(wasManualRefresh != isManualRefresh());

	if (isManualRefresh()) {
		Quirk.waitVblank = false;
		mTargetCPF = cInstSpeedMC;
	}
	else {
		isLargerDisplay(mode != Resolution::LO);

		Quirk.waitVblank = !isLargerDisplay();
		mTargetCPF = isLargerDisplay() ? cInstSpeedLo : cInstSpeedHi;
	}
};

/*==================================================================*/

void MEGACHIP::skipInstruction() noexcept {
	mCurrentPC += readMemory(mCurrentPC) == 0x01 ? 4 : 2;
}

void MEGACHIP::scrollDisplayUP(s32 N) {
	mDisplayBuffer.shift(0, -N);
}
void MEGACHIP::scrollDisplayDN(s32 N) {
	mDisplayBuffer.shift(0, +N);
}
void MEGACHIP::scrollDisplayLT() {
	mDisplayBuffer.shift(-4, 0);
}
void MEGACHIP::scrollDisplayRT() {
	mDisplayBuffer.shift(+4, 0);
}

/*==================================================================*/

void MEGACHIP::initializeFontColors() noexcept {
	for (auto i{ 0 }; i < 10; ++i) {
		const auto mult{ u8(std::max(0, 255 - 11 * i)) };

		mFontColor[i] = RGBA{
			ez::fixedScale8(mult, 264),
			ez::fixedScale8(mult, 291),
			ez::fixedScale8(mult, 309),
		};
	}
}

void MEGACHIP::selectBlendingAlgo(s32 mode) noexcept {
	switch (mode) {
		case BlendMode::LINEAR_DODGE:
			mBlendFunc = RGBA::Blend::LinearDodge;
			break;

		case BlendMode::MULTIPLY:
			mBlendFunc = RGBA::Blend::Multiply;
			break;

		default:
		case BlendMode::ALPHA_BLEND:
			mBlendFunc = RGBA::Blend::None;
			break;
	}
}

void MEGACHIP::scrapAllVideoBuffers() {
	mLastRenderBuffer.initialize();
	mBackgroundBuffer.initialize();
	mCollisionMap.initialize();
}

void MEGACHIP::flushAllVideoBuffers() {
	BVS->displayBuffer.write(mBackgroundBuffer);

	mLastRenderBuffer = mBackgroundBuffer;
	mBackgroundBuffer.initialize();
	mCollisionMap.initialize();
}

void MEGACHIP::blendAndFlushBuffers() const {
	BVS->displayBuffer.write(
		mLastRenderBuffer,
		mBackgroundBuffer,
		RGBA::blendAlpha
	);
}

void MEGACHIP::startAudioTrack(bool repeat) noexcept {
	if (auto* stream{ mAudioDevice.at(STREAM::MAIN) }) {

		mTrack.loop = repeat;
		mTrack.data = &mMemoryBank[mRegisterI + 6];
		mTrack.size = readMemoryI(2) << 16
					| readMemoryI(3) <<  8
					| readMemoryI(4);

		const bool oob{ mTrack.data + mTrack.size > &mMemoryBank.back() };
		if (!mTrack.size || oob) { mTrack.reset(); }
		else {
			mVoices[VOICE::UNIQUE].setPhase(0.0).setStep(getFramerateMultiplier() * (
				(readMemoryI(0) << 8 | readMemoryI(1)) / f64(mTrack.size) / stream->getFreq()
			)).userdata = &mTrack;
		}
	}
}

void MEGACHIP::makeByteWave(f32* data, u32 size, Voice* voice, Stream*) noexcept {
	if (!voice || !voice->userdata) [[unlikely]] { return; }
	if (auto* track{ static_cast<TrackData*>(voice->userdata) }) {
		if (!track->isOn()) { return; }

		for (auto i{ 0u }; i < size; ++i) {
			const auto head{ voice->peekRawPhase(i) };
			if (!track->loop && head >= 1.0) {
				track->reset(); return;
			} else {
				::assign_cast_add(data[i], (1.0 / 128) * \
					track->pos(head));
			}
		}
		voice->stepPhase(size);
	}
}

void MEGACHIP::scrollBuffersUP(s32 N) {
	mLastRenderBuffer.shift(0, -N);
	blendAndFlushBuffers();
}
void MEGACHIP::scrollBuffersDN(s32 N) {
	mLastRenderBuffer.shift(0, +N);
	blendAndFlushBuffers();
}
void MEGACHIP::scrollBuffersLT() {
	mLastRenderBuffer.shift(-4, 0);
	blendAndFlushBuffers();
}
void MEGACHIP::scrollBuffersRT() {
	mLastRenderBuffer.shift(+4, 0);
	blendAndFlushBuffers();
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void MEGACHIP::instruction_00BN(s32 N) noexcept {
		if (isManualRefresh()) {
			scrollBuffersUP(N);
		} else {
			scrollDisplayUP(N);
		}
	}
	void MEGACHIP::instruction_00CN(s32 N) noexcept {
		if (isManualRefresh()) {
			scrollBuffersDN(N);
		} else {
			scrollDisplayDN(N);
		}
	}
	void MEGACHIP::instruction_00E0() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		if (isManualRefresh()) {
			flushAllVideoBuffers();
		} else {
			mDisplayBuffer.initialize();
		}
	}
	void MEGACHIP::instruction_00EE() noexcept {
		mCurrentPC = mStackBank[--mStackTop & 0xF];
	}
	void MEGACHIP::instruction_00FB() noexcept {
		if (isManualRefresh()) {
			scrollBuffersRT();
		} else {
			scrollDisplayRT();
		}
	}
	void MEGACHIP::instruction_00FC() noexcept {
		if (isManualRefresh()) {
			scrollBuffersLT();
		} else {
			scrollDisplayLT();
		}
	}
	void MEGACHIP::instruction_00FD() noexcept {
		triggerInterrupt(Interrupt::SOUND);
	}
	void MEGACHIP::instruction_00FE() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		prepDisplayArea(Resolution::LO);
	}
	void MEGACHIP::instruction_00FF() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		prepDisplayArea(Resolution::HI);
	}

	void MEGACHIP::instruction_0010() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		prepDisplayArea(Resolution::LO);
		scrapAllVideoBuffers();
	}
	void MEGACHIP::instruction_0011() noexcept {
		triggerInterrupt(Interrupt::FRAME);
		prepDisplayArea(Resolution::MC);

		selectBlendingAlgo(BlendMode::ALPHA_BLEND);
		initializeFontColors();
		scrapAllVideoBuffers();

		mTexture.reset();
		mTrack.reset();
	}
	void MEGACHIP::instruction_01NN(s32 NN) noexcept {
		setIndexRegister((NN << 16) | NNNN());
		::assign_cast_add(mCurrentPC, 2);
	}
	void MEGACHIP::instruction_02NN(s32 NN) noexcept {
		for (auto pos{ 0 }, byte{ 0 }; pos < NN; byte += 4) {
			mColorPalette(++pos) = {
				readMemoryI(byte + 1),
				readMemoryI(byte + 2),
				readMemoryI(byte + 3),
				readMemoryI(byte + 0),
			};
		}
	}
	void MEGACHIP::instruction_03NN(s32 NN) noexcept {
		mTexture.W = NN ? NN : 256;
	}
	void MEGACHIP::instruction_04NN(s32 NN) noexcept {
		mTexture.H = NN ? NN : 256;
	}
	void MEGACHIP::instruction_05NN(s32 NN) noexcept {
		BVS->setViewportAlpha(NN);
	}
	void MEGACHIP::instruction_060N(s32 N) noexcept {
		startAudioTrack(N == 0);
	}
	void MEGACHIP::instruction_0700() noexcept {
		mTrack.reset();
	}
	void MEGACHIP::instruction_080N(s32 N) noexcept {
		static constexpr u8 opacity[]{ 0xFF, 0x3F, 0x7F, 0xBF };
		::assign_cast(mTexture.opacity, opacity[N > 3 ? 0 : N]);
		selectBlendingAlgo(N);
	}
	void MEGACHIP::instruction_09NN(s32 NN) noexcept {
		mTexture.collide = NN;
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void MEGACHIP::instruction_1NNN(s32 NNN) noexcept {
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void MEGACHIP::instruction_2NNN(s32 NNN) noexcept {
		mStackBank[mStackTop++ & 0xF] = mCurrentPC;
		performProgJump(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void MEGACHIP::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] == NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void MEGACHIP::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (mRegisterV[X] != NN) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void MEGACHIP::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] == mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void MEGACHIP::instruction_6xNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void MEGACHIP::instruction_7xNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], mRegisterV[X] + NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void MEGACHIP::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(mRegisterV[X], mRegisterV[Y]);
	}
	void MEGACHIP::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(mRegisterV[X], mRegisterV[Y]);
	}
	void MEGACHIP::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(mRegisterV[X], mRegisterV[Y]);
	}
	void MEGACHIP::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(mRegisterV[X], mRegisterV[Y]);
	}
	void MEGACHIP::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum{ mRegisterV[X] + mRegisterV[Y] };
		::assign_cast(mRegisterV[X], sum);
		::assign_cast(mRegisterV[0xF], sum >> 8);
	}
	void MEGACHIP::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[X] >= mRegisterV[Y] };
		::assign_cast_sub(mRegisterV[X], mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void MEGACHIP::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow{ mRegisterV[Y] >= mRegisterV[X] };
		::assign_cast_rsub(mRegisterV[X], mRegisterV[Y]);
		::assign_cast(mRegisterV[0xF], nborrow);
	}
	void MEGACHIP::instruction_8xy6(s32 X, s32  ) noexcept {
		const bool lsb{ (mRegisterV[X] & 0x01) != 0 };
		::assign_cast_shr(mRegisterV[X], 1);
		::assign_cast(mRegisterV[0xF], lsb);
	}
	void MEGACHIP::instruction_8xyE(s32 X, s32  ) noexcept {
		const bool msb{ (mRegisterV[X] & 0x80) != 0 };
		::assign_cast_shl(mRegisterV[X], 1);
		::assign_cast(mRegisterV[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void MEGACHIP::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (mRegisterV[X] != mRegisterV[Y]) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void MEGACHIP::instruction_ANNN(s32 NNN) noexcept {
		setIndexRegister(NNN & 0xFFF);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void MEGACHIP::instruction_BXNN(s32 X, s32 NNN) noexcept {
		performProgJump(NNN + mRegisterV[X]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void MEGACHIP::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(mRegisterV[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	bool MEGACHIP::drawSingleBytes(
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

	bool MEGACHIP::drawDoubleBytes(
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
				collided |= !!(pixelHI & 0x8);
				pixelLO = pixelHI ^= 0x8;
			} else {
				pixelLO = pixelHI;
			}
			if (offsetX == cScreenSizeX - 1) { return collided; }
		}
		return collided;
	}

	void MEGACHIP::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		if (Quirk.waitVblank) [[unlikely]]
			{ triggerInterrupt(Interrupt::FRAME); }

		if (isManualRefresh()) {
			const auto originX{ mRegisterV[X] + 0 };
			const auto originY{ mRegisterV[Y] + 0 };

			mRegisterV[0xF] = 0;

			if (!Quirk.wrapSprite && originY >= cScreenMegaY) { return; }
			if (mTexture.fontOffset != mRegisterI) [[likely]] { goto paintTexture; }

			for (auto rowN{ 0 }, offsetY{ originY }; rowN < N; ++rowN)
			{
				if (Quirk.wrapSprite && offsetY >= cScreenMegaY) { continue; }
				const auto octoPixelBatch{ readMemoryI(rowN) };

				for (auto colN{ 7 }, offsetX{ originX }; colN >= 0; --colN)
				{
					if (octoPixelBatch >> colN & 0x1)
						{ mBackgroundBuffer(offsetX, offsetY) = mFontColor[rowN]; }

					if (!Quirk.wrapSprite && offsetX == (cScreenMegaX - 1))
						{ break; } else { ++offsetX &= (cScreenMegaX - 1); }
				}
				if (!Quirk.wrapSprite && offsetY == (cScreenMegaX - 1))
					{ break; } else { ++offsetY &= (cScreenMegaX - 1); }
			}
			return;

		paintTexture:
			if (mRegisterI + mTexture.W * mTexture.H >= cTotalMemory)
				[[unlikely]] { mTexture.reset(); return; }

			for (auto rowN{ 0 }, offsetY{ originY }; rowN < mTexture.H; ++rowN)
			{
				if (Quirk.wrapSprite && offsetY >= cScreenMegaY) { continue; }
				const auto offsetI = rowN * mTexture.W;

				for (auto colN{ 0 }, offsetX{ originX }; colN < mTexture.W; ++colN)
				{
					if (const auto sourceColorIdx{ readMemoryI(offsetI + colN) })
					{
						auto& collideCoord{ mCollisionMap(offsetX, offsetY) };
						auto& backbufCoord{ mBackgroundBuffer(offsetX, offsetY) };

						if (collideCoord == mTexture.collide)
							[[unlikely]] { mRegisterV[0xF] = 1; }

						collideCoord = sourceColorIdx;
						backbufCoord = RGBA::compositeBlend(mColorPalette(sourceColorIdx), \
							backbufCoord, mBlendFunc, u8(mTexture.opacity));
					}
					if (!Quirk.wrapSprite && offsetX == (cScreenMegaX - 1))
						{ break; } else { ++offsetX &= (cScreenMegaX - 1); }
				}
				if (!Quirk.wrapSprite && offsetY == (cScreenMegaY - 1))
					{ break; } else { ++offsetY %= cScreenMegaY; }
			}
		} else {
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
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void MEGACHIP::instruction_Ex9E(s32 X) noexcept {
		if (keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}
	void MEGACHIP::instruction_ExA1(s32 X) noexcept {
		if (!keyHeld_P1(mRegisterV[X])) { skipInstruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void MEGACHIP::instruction_Fx07(s32 X) noexcept {
		::assign_cast(mRegisterV[X], mDelayTimer);
	}
	void MEGACHIP::instruction_Fx0A(s32 X) noexcept {
		triggerInterrupt(Interrupt::INPUT);
		mInputReg = &mRegisterV[X];
		if (isManualRefresh()) [[unlikely]]
			{ BVS->displayBuffer.write(mBackgroundBuffer); }
	}
	void MEGACHIP::instruction_Fx15(s32 X) noexcept {
		mDelayTimer = mRegisterV[X];
	}
	void MEGACHIP::instruction_Fx18(s32 X) noexcept {
		startVoiceAt(VOICE::BUZZER, mRegisterV[X] + (mRegisterV[X] == 1));
	}
	void MEGACHIP::instruction_Fx1E(s32 X) noexcept {
		incIndexRegister(mRegisterV[X]);
	}
	void MEGACHIP::instruction_Fx29(s32 X) noexcept {
		setIndexRegister((mRegisterV[X] & 0xF) * 5 + cSmallFontOffset);
		mTexture.fontOffset = mRegisterI;
	}
	void MEGACHIP::instruction_Fx30(s32 X) noexcept {
		setIndexRegister((mRegisterV[X] & 0xF) * 10 + cLargeFontOffset);
		mTexture.fontOffset = mRegisterI;
	}
	void MEGACHIP::instruction_Fx33(s32 X) noexcept {
		const TriBCD bcd{ mRegisterV[X] };

		writeMemoryI(bcd.digit[2], 0);
		writeMemoryI(bcd.digit[1], 1);
		writeMemoryI(bcd.digit[0], 2);
	}
	void MEGACHIP::instruction_FN55(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx)
			{ writeMemoryI(mRegisterV[idx], idx); }
	}
	void MEGACHIP::instruction_FN65(s32 N) noexcept {
		for (auto idx{ 0 }; idx <= N; ++idx)
			{ mRegisterV[idx] = readMemoryI(idx); }
	}
	void MEGACHIP::instruction_FN75(s32 N) noexcept {
		setPermaRegs(std::min(N, 7) + 1);
	}
	void MEGACHIP::instruction_FN85(s32 N) noexcept {
		getPermaRegs(std::min(N, 7) + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
