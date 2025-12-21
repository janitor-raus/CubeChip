/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#define ENABLE_CHIP8_SYSTEM
#ifdef ENABLE_CHIP8_SYSTEM

#include "../SystemInterface.hpp"

/*==================================================================*/

class Chip8_CoreInterface : public SystemInterface {

protected:
	enum STREAM : u32 { MAIN };
	enum VOICE : u32 {
		ID_0, ID_1, ID_2, ID_3, COUNT,
		BUZZER = ID_3, UNIQUE = ID_0,
	};

	static inline thread_local Path sPermaRegsPath{};
	static inline thread_local Path sSavestatePath{};
	static constexpr f32 sTonalOffset = 160.0f;

	struct TriBCD final {
		// 2 = hundreds | 1 = tens | 0 = ones
		u8 digit[3];

		constexpr TriBCD(u32 value) noexcept {
			::assign_cast(digit[2], value * 0x51EB851Full >> 37);
			::assign_cast(temp_val, value - digit[2] * 100);
			::assign_cast(digit[1], temp_val * 0xCCCDull >> 19);
			::assign_cast(digit[0], temp_val - digit[1] * 10);
		}

	private:
		u32 temp_val;
	};

/*==================================================================*/

	DisplayDevice mDisplayDevice;

	AudioDevice mAudioDevice;

	std::array<Voice, VOICE::COUNT>
		mVoices{};

	std::array<AudioTimer, VOICE::COUNT>
		mAudioTimers{};

	void startVoice(s32 duration, s32 tone = 0) noexcept;
	void startVoiceAt(u32 voice_index, u32 duration, u32 tone = 0) noexcept;

	void mixAudioData(VoiceGenerators processors) noexcept;
	static void makePulseWave(f32* data, u32 size, Voice* voice, Stream* stream) noexcept;

/*==================================================================*/

	std::vector<SimpleKeyMapping> mCustomBinds;

private:
	u32  mTickLast{};
	u32  mTickSpan{};

	u32  mKeysCurr{}; // bitfield of key states in current frame
	u32  mKeysPrev{}; // bitfield of key states in previous frame
	u32  mKeysLock{}; // bitfield of keys excluded from input checks
	u32  mKeysLoop{}; // bitfield of keys repeating input on Fx0A

protected:
	void updateKeyStates() noexcept;
	void loadPresetBinds() noexcept;

	template <IsContiguousContainer T>
		requires (SameValueTypes<T, decltype(mCustomBinds)>)
	void loadCustomBinds(const T& binds) noexcept {
		mCustomBinds.assign(std::begin(binds), std::end(binds));
		mKeysPrev = mKeysCurr = mKeysLock = 0;
	}

	bool keyPressed(u8* keyReg) noexcept;
	bool keyHeld_P1(u32 keyIndex) const noexcept;
	bool keyHeld_P2(u32 keyIndex) const noexcept;

/*==================================================================*/

	struct alignas(int) PlatformQuirks final {
		bool clearVF{};
		bool jmpRegX{};
		bool shiftVX{};
		bool idxRegNoInc{};
		bool idxRegMinus{};
		bool waitVblank{};
		bool waitScroll{};
		bool wrapSprite{};
	} Quirk;

	struct alignas(int) PlatformTraits final {
		bool largerDisplay{};
		bool manualRefresh{};
		bool usingPixelTrails{};
	} Trait;

	enum class Interrupt {
		CLEAR, // no interrupt
		FRAME, // single-frame
		SOUND, // wait for sound and stop
		DELAY, // wait for delay and proceed
		INPUT, // wait for input and proceed
		WAIT1, // intermediate wait state towards FINAL
		FINAL, // end state, all is well
		ERROR, // end state, error occured
	};

	bool hasInterrupt() const noexcept { return mInterrupt != Interrupt::CLEAR; }

	enum class Resolution {
		ERROR,
		HI, // 128 x  64 (display ratio 2:1)
		LO, //  64 x  32 (display ratio 2:1)
		TP, //  64 x  64 (display ratio 2:1)
		FP, //  64 x 128 (display ratio 2:1)
		MC, // 256 x 192 (display ratio 4:3)
	};

/*==================================================================*/

	bool isLargerDisplay()     const noexcept { return Trait.largerDisplay; }
	bool isManualRefresh()     const noexcept { return Trait.manualRefresh; }
	bool isUsingPixelTrails()  const noexcept { return Trait.usingPixelTrails; }
	bool isLargerDisplay    (bool state) noexcept { return std::exchange(Trait.largerDisplay, state);     }
	bool isManualRefresh    (bool state) noexcept { return std::exchange(Trait.manualRefresh, state);     }
	bool isUsingPixelTrails (bool state) noexcept { return std::exchange(Trait.usingPixelTrails, state);  }

/*==================================================================*/

	u32 mTargetCPF{};
	u32 mCycleCount{};
	Interrupt mInterrupt{};

	u32 mCurrentPC{};
	u32 mRegisterI{};

	u8* mInputReg{};

	u32 mDelayTimer{};
	//u32 mKeyPitch{};

	u32 mStackTop{};

	static inline thread_local
	std::array<u8, 16>
		sPermRegsV{};

	std::array<u8, 16>
		mRegisterV{};

	alignas(HDIS)
	std::array<u32, 16>
		mStackBank{};

/*==================================================================*/

	void instructionError(u32 HI, u32 LO);
	void triggerInterrupt(Interrupt type) noexcept;

private:
	bool checkRegularFile(const Path& filePath) const noexcept;
	bool newPermaRegsFile(const Path& filePath) const noexcept;

	void setFilePermaRegs(u32 X) noexcept;
	void getFilePermaRegs(u32 X) noexcept;

protected:
	void setPermaRegs(u32 X) noexcept;
	void getPermaRegs(u32 X) noexcept;

	void copyGameToMemory(void* dest) noexcept;
	void copyFontToMemory(void* dest, size_type size) noexcept;
	void copyColorsToCore(void* dest) noexcept;

	/*   */ void handlePreFrameInterrupt() noexcept;
	/*   */ void handleEndFrameInterrupt() noexcept;

	/*   */ void handleTimerTick() noexcept;
	virtual void handleCycleLoop() noexcept = 0;

	virtual void skipInstruction() noexcept;
	/*   */ void performProgJump(u32 next) noexcept;
	virtual void prepDisplayArea(Resolution mode) = 0;

	virtual void renderAudioData() = 0;
	virtual void renderVideoData() = 0;

#define LOOP_DISPATCH(LOOP_FUNCTION)					\
	if (hasSystemState(EmuState::BENCH)) [[likely]] {	\
		setStopFrame(hasInterrupt());					\
		LOOP_FUNCTION([&]() noexcept					\
			{ return !getStopFrame(); });				\
	} else {											\
		LOOP_FUNCTION([&]() noexcept					\
			{ return !hasInterrupt() && mCycleCount < mTargetCPF; }); \
	}

protected:
	Chip8_CoreInterface(DisplayDevice display_device) noexcept;

public:
	void mainSystemLoop() override final;

	void appendOverlayData() noexcept override final;

/*==================================================================*/

protected:
	static constexpr auto cSmallFontOffset = 0x00;
	static constexpr auto cLargeFontOffset = 0x50;

	static constexpr std::array<u8, 240> cFontsData = {
		0x60, 0xA0, 0xA0, 0xA0, 0xC0, // 0
		0x40, 0xC0, 0x40, 0x40, 0xE0, // 1
		0xC0, 0x20, 0x40, 0x80, 0xE0, // 2
		0xC0, 0x20, 0x40, 0x20, 0xC0, // 3
		0x20, 0xA0, 0xE0, 0x20, 0x20, // 4
		0xE0, 0x80, 0xC0, 0x20, 0xC0, // 5
		0x40, 0x80, 0xC0, 0xA0, 0x40, // 6
		0xE0, 0x20, 0x60, 0x40, 0x40, // 7
		0x40, 0xA0, 0x40, 0xA0, 0x40, // 8
		0x40, 0xA0, 0x60, 0x20, 0x40, // 9
		0x40, 0xA0, 0xE0, 0xA0, 0xA0, // A
		0xC0, 0xA0, 0xC0, 0xA0, 0xC0, // B
		0x60, 0x80, 0x80, 0x80, 0x60, // C
		0xC0, 0xA0, 0xA0, 0xA0, 0xC0, // D
		0xE0, 0x80, 0xC0, 0x80, 0xE0, // E
		0xE0, 0x80, 0xC0, 0x80, 0x80, // F

		0x7C, 0xC6, 0xCE, 0xDE, 0xD6, 0xF6, 0xE6, 0xC6, 0x7C, 0x00, // 0
		0x10, 0x30, 0xF0, 0x30, 0x30, 0x30, 0x30, 0x30, 0xFC, 0x00, // 1
		0x78, 0xCC, 0xCC, 0x0C, 0x18, 0x30, 0x60, 0xCC, 0xFC, 0x00, // 2
		0x78, 0xCC, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0xCC, 0x78, 0x00, // 3
		0x0C, 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x1E, 0x00, // 4
		0xFC, 0xC0, 0xC0, 0xC0, 0xF8, 0x0C, 0x0C, 0xCC, 0x78, 0x00, // 5
		0x38, 0x60, 0xC0, 0xC0, 0xF8, 0xCC, 0xCC, 0xCC, 0x78, 0x00, // 6
		0xFE, 0xC6, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00, // 7
		0x78, 0xCC, 0xCC, 0xEC, 0x78, 0xDC, 0xCC, 0xCC, 0x78, 0x00, // 8
		0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x18, 0x18, 0x30, 0x70, 0x00, // 9
		/* ------ omit segment below if legacy superchip ------ */
		0x30, 0x78, 0xCC, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0xCC, 0x00, // A
		0xFC, 0x66, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x66, 0xFC, 0x00, // B
		0x3C, 0x66, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x66, 0x3C, 0x00, // C
		0xF8, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00, // D
		0xFE, 0x62, 0x60, 0x64, 0x7C, 0x64, 0x60, 0x62, 0xFE, 0x00, // E
		0xFE, 0x66, 0x62, 0x64, 0x7C, 0x64, 0x60, 0x60, 0xF0, 0x00, // F
	};
	static inline thread_local std::array<u8, 240> sFontsData = cFontsData;

	static constexpr std::array<RGBA, 16> cBitColors = { // 0-1 monochrome, 0-15 palette color
		0x181C20FF, 0xE4DCD4FF, 0x8C8884FF, 0x403C38FF,
		0xD82010FF, 0x40D020FF, 0x1040D0FF, 0xE0C818FF,
		0x501010FF, 0x105010FF, 0x50B0C0FF, 0xF08010FF,
		0xE06090FF, 0xE0F090FF, 0xB050F0FF, 0x704020FF,
	};
	static inline thread_local std::array<RGBA, 16> sBitColors = cBitColors;

	static constexpr std::array<RGBA,  8> cForeColor = { // CHIP-8X foreground colors
		0x000000FF, 0xEE1111FF, 0x1111EEFF, 0xEE11EEFF,
		0x11EE11FF, 0xEEEE11FF, 0x11EEEEFF, 0xEEEEEEFF,
	};
	static constexpr std::array<RGBA,  4> cBackColor = { // CHIP-8X background colors
		0x111133FF, 0x111111FF, 0x113311FF, 0x331111FF,
	};

/*==================================================================*/

	#define PX_1 0x37
	#define PX_2 0x67
	#define PX_3 0xE7
	#define PX_4 0xFF

	// Premul table for pixel trails
	static constexpr std::array<u32, 16> cBitWeight = {
		PX_4, PX_1, PX_2, PX_2,
		PX_3, PX_3, PX_3, PX_3,
		PX_4, PX_4, PX_4, PX_4,
		PX_4, PX_4, PX_4, PX_4,
	};

	#undef PX_1
	#undef PX_2
	#undef PX_3
	#undef PX_4
};

#endif
