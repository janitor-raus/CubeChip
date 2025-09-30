/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../GameBoy_CoreInterface.hpp"
#include "BasicLogger.hpp"

#define ENABLE_GAMEBOY_CLASSIC
#if defined(ENABLE_GAMEBOY_SYSTEM) && defined(ENABLE_GAMEBOY_CLASSIC)

#include <initializer_list>

/*==================================================================*/

class GAMEBOY_CLASSIC final : public GameBoy_CoreInterface {
	static constexpr u64 cTotalMemory{ KiB(64) };
	static constexpr u32 cSafezoneOOB{        8 };
	static constexpr f32 cRefreshRate{ 59.7275f };
	static constexpr s32 cResSizeMult{        2 };
	static constexpr s32 cScreenSizeX{      160 };
	static constexpr s32 cScreenSizeY{      144 };
	static constexpr s32 cCylesPerSec{  4194304 };
	static constexpr s32 cScreenSizeT{    23040 };

private:
	void instructionLoop() noexcept override;
	void renderAudioData() override;
	void renderVideoData() override;

	class MMU {
	public:


	std::array<u8, cTotalMemory> mMemoryBanks{};

		/* Memory Map */
		std::span<u8, 0x0100> mBootRomBank{ mMemoryBanks.begin() + 0x0000, 0x0100 }; // BOOT ROM
		std::span<u8, 0x4000> mRomBank00  { mMemoryBanks.begin() + 0x0000, 0x4000 }; // ROM BANK 0
		std::span<u8, 0x4000> mRomBankNN  { mMemoryBanks.begin() + 0x4000, 0x4000 }; // ROM BANK N
		std::span<u8, 0x2000> mVideoBank  { mMemoryBanks.begin() + 0x8000, 0x2000 }; // VRAM
		std::span<u8, 0x2000> mExtBank    { mMemoryBanks.begin() + 0xA000, 0x2000 }; // EXT RAM
		std::span<u8, 0x1000> mWorkBank0  { mMemoryBanks.begin() + 0xC000, 0x1000 }; // WRAM 0
		std::span<u8, 0x1000> mWorkBankN  { mMemoryBanks.begin() + 0xD000, 0x1000 }; // WRAM N
		std::span<u8, 0x1E00> mEchoBank   { mMemoryBanks.begin() + 0xE000, 0x1E00 }; // ECHO RAM (C000-DDFF)
		std::span<u8, 0x00A0> mObjAttrBank{ mMemoryBanks.begin() + 0xFE00, 0x00A0 }; // OAM
		//std::span<u8, 0x0060> mProhibited { mMemoryBanks.begin() + 0xFEA0, 0x0060 }; // PROHIBITED
		std::span<u8, 0x0080> mInOutBank  { mMemoryBanks.begin() + 0xFF00, 0x0080 }; // IO REGS
		std::span<u8, 0x007F> mHighBank   { mMemoryBanks.begin() + 0xFF80, 0x007F }; // HRAM

		/* Video Bank 0 Tile Map */
		std::span<u8, 0x0800> mVideoTileMap0{ mVideoBank.begin() + 0x0000, 0x0800 };
		std::span<u8, 0x0800> mVideoTileMap1{ mVideoBank.begin() + 0x0800, 0x0800 };
		std::span<u8, 0x0800> mVideoTileMap2{ mVideoBank.begin() + 0x1000, 0x0800 };
		std::span<u8, 0x0400> mVideoTileMap3{ mVideoBank.begin() + 0x1800, 0x0400 };
		std::span<u8, 0x0400> mVideoTileMap4{ mVideoBank.begin() + 0x1C00, 0x0400 };

		/* Video Bank 1 Attr Map */
		std::span<u8, 0x0400> mVideoAttrMap0{ mVideoBank.begin() + 0x1800, 0x0400 };
		std::span<u8, 0x0400> mVideoAttrMap1{ mVideoBank.begin() + 0x1C00, 0x0400 };


		/* I/O Ranges */
		u8& mJOYP { mInOutBank[0x00] }; // Joypad
		u8& mSB   { mInOutBank[0x01] }; // Serial transfer data
		u8& mSC   { mInOutBank[0x02] }; // Serial transfer control

		u8& mDIV  { mInOutBank[0x04] }; // Divider register
		u8& mTIMA { mInOutBank[0x05] }; // Timer counter
		u8& mTMA  { mInOutBank[0x06] }; // Timer modulo
		u8& mTAC  { mInOutBank[0x07] }; // Timer control

		u8& mIF   { mInOutBank[0x0F] }; // Interrupt flag

		u8& mNR10 { mInOutBank[0x10] }; // Sound channel 1 sweep
		u8& mNR11 { mInOutBank[0x11] }; // Sound channel 1 length timer & duty cycle
		u8& mNR12 { mInOutBank[0x12] }; // Sound channel 1 volume & envelope
		u8& mNR13 { mInOutBank[0x13] }; // Sound channel 1 period low
		u8& mNR14 { mInOutBank[0x14] }; // Sound channel 1 period high & control

		u8& mNR21 { mInOutBank[0x16] }; // Sound channel 2 length timer & duty cycle
		u8& mNR22 { mInOutBank[0x17] }; // Sound channel 2 volume & envelope
		u8& mNR23 { mInOutBank[0x18] }; // Sound channel 2 period low
		u8& mNR24 { mInOutBank[0x19] }; // Sound channel 2 period high & control

		u8& mNR30 { mInOutBank[0x1A] }; // Sound channel 3 DAC enable
		u8& mNR31 { mInOutBank[0x1B] }; // Sound channel 3 length timer
		u8& mNR32 { mInOutBank[0x1C] }; // Sound channel 3 output level
		u8& mNR33 { mInOutBank[0x1D] }; // Sound channel 3 period low
		u8& mNR34 { mInOutBank[0x1E] }; // Sound channel 3 period high & control

		u8& mNR40 { mInOutBank[0x20] }; // Sound channel 4 length timer
		u8& mNR41 { mInOutBank[0x21] }; // Sound channel 4 volume & envelope
		u8& mNR42 { mInOutBank[0x22] }; // Sound channel 4 frequency & randomness
		u8& mNR43 { mInOutBank[0x23] }; // Sound channel 4 control

		u8& mNR50 { mInOutBank[0x24] }; // Master volume & VIN panning
		u8& mNR51 { mInOutBank[0x25] }; // Sound panning
		u8& mNR52 { mInOutBank[0x26] }; // Sound on/off

		std::span<u8, 0x10> mWAVE /* Storage for waveform */
			{ mInOutBank.begin() + 0x30, 0x10 };

		u8& mLCDC { mInOutBank[0x40] }; // LCD control
		u8& mSTAT { mInOutBank[0x41] }; // LCD status
		u8& mSCY  { mInOutBank[0x42] }; // Viewport Y pos
		u8& mSCX  { mInOutBank[0x43] }; // Viewport X pos
		u8& mLY   { mInOutBank[0x44] }; // LCD Y coord
		u8& mLYC  { mInOutBank[0x45] }; // LY compare
		u8& mDMA  { mInOutBank[0x46] }; // OAM DMA source addr & start
		u8& mBGP  { mInOutBank[0x47] }; // BG palette data
		u8& mOBP0 { mInOutBank[0x48] }; // OBJ palette 0 data
		u8& mOBP1 { mInOutBank[0x49] }; // OBJ palette 1 data
		u8& mWY   { mInOutBank[0x4A] }; // Window Y pos
		u8& mWX   { mInOutBank[0x4B] }; // Window X pos + 7
		u8& mKEY1 { mInOutBank[0x4D] }; // Prepare speed switch

		u8& mVBK  { mInOutBank[0x4F] }; // VRAM bank
		u8& mBOOT { mInOutBank[0x50] }; // Boot ROM enable
		u8& mHDMA1{ mInOutBank[0x51] }; // VRAM DMA src hi
		u8& mHDMA2{ mInOutBank[0x52] }; // VRAM DMA src lo
		u8& mHDMA3{ mInOutBank[0x53] }; // VRAM DMA dst hi
		u8& mHDMA4{ mInOutBank[0x54] }; // VRAM DMA dst lo
		u8& mHDMA5{ mInOutBank[0x55] }; // VRAM DMA len/mode/start

		u8& mRP   { mInOutBank[0x56] }; // Infrared comms port

		u8& mBCPS { mInOutBank[0x68] }; // BG (color) palette spec / index
		u8& mBCPD { mInOutBank[0x69] }; // BG (color) palette data
		u8& mOCPS { mInOutBank[0x6A] }; // OBJ (color) palette spec / index
		u8& mOCPD { mInOutBank[0x6B] }; // OBJ (color) palette data
		u8& mOPRI { mInOutBank[0x6C] }; // OBJ priority mode
		u8& mSVBK { mInOutBank[0x70] }; // WRAM bank

		u8& mPCM12{ mInOutBank[0x76] }; // Audio digital out 1 & 2
		u8& mPCM34{ mInOutBank[0x77] }; // Audio digital out 3 & 4

		u8& mIE   { mMemoryBanks[0xFFFF] }; // Interrupt enable
	} mMMU;

	u8 mInputControl{};
	u8 mInputData[2]{};

	void updateKeyStates() noexcept override {
		const auto keyState{ static_cast<u8>(getKeyStates()) };
		const auto keysDPAD{ static_cast<u8>(keyState & 0xF) };
		const auto keysBTNS{ static_cast<u8>(keyState >>  4) };
		const auto currJOYP{ static_cast<u8>(~mMMU.mJOYP) };

		if (keysDPAD & ~(currJOYP & 0xF) && currJOYP & 0x10)
			{ /* trigger CPU interrupt 0x60 */ }
		if (keysBTNS & ~(currJOYP & 0xF) && currJOYP & 0x20)
			{ /* trigger CPU interrupt 0x60 */ }

		mInputData[0] = ~keysDPAD & 0xF;
		mInputData[1] = ~keysBTNS & 0xF;
	}

	void setJOYP(u32 addr, u32 value) noexcept {
		if (addr != 0xFF00) {
			blog.newEntry<BLOG::WARN>("JoyPad cannot write to 0x{:04X}", addr);
			return;
		} else [[likely]] {
			mInputControl = value & 0x30;
		}
	}

	u32 getJOYP(u32 addr) const noexcept {
		if (addr != 0xFF00) {
			blog.newEntry<BLOG::WARN>("JoyPad cannot read from 0x{:04X}", addr);
			return 0;
		} else [[likely]] {
			switch (mInputControl) {
				case 0x10:
					return mInputData[0] | 0xC0;
					break;
				case 0x20:
					return mInputData[1] | 0xC0;
					break;
				default:
					return 0xCF;
			}
		}
	}

	class PPU {
		// insert spongebob in box saying "IMAGINATION!"
	} mPPU;


	class CPU {
		enum RegChar {
			A, B, C, D, E, F, H, L,
			AF, BC, DE, HL
		};
		class Registers {
			u8 mA{}, mB{}, mC{}, mD{};
			u8 mE{}, mF{}, mH{}, mL{};

		public:
			template <RegChar T>
			void set(u32 value) noexcept {
				       if constexpr (T == A) {
					mA = static_cast<u8>(value);
				} else if constexpr (T == B) {
					mB = static_cast<u8>(value);
				} else if constexpr (T == C) {
					mC = static_cast<u8>(value);
				} else if constexpr (T == D) {
					mD = static_cast<u8>(value);
				} else if constexpr (T == E) {
					mE = static_cast<u8>(value);
				} else if constexpr (T == F) {
					mF = static_cast<u8>(value);
				} else if constexpr (T == H) {
					mH = static_cast<u8>(value);
				} else if constexpr (T == L) {
					mL = static_cast<u8>(value);
				} else if constexpr (T == AF) {
					mA = static_cast<u8>(value >> 8);
					mF = static_cast<u8>(value >> 0);
				} else if constexpr (T == BC) {
					mB = static_cast<u8>(value >> 8);
					mC = static_cast<u8>(value >> 0);
				} else if constexpr (T == DE) {
					mD = static_cast<u8>(value >> 8);
					mE = static_cast<u8>(value >> 0);
				} else if constexpr (T == HL) {
					mH = static_cast<u8>(value >> 8);
					mL = static_cast<u8>(value >> 0);
				}
			}

			template <RegChar T>
			u32  get() const noexcept {
				       if constexpr (T == A) {
					return mA;
				} else if constexpr (T == B) {
					return mB;
				} else if constexpr (T == C) {
					return mC;
				} else if constexpr (T == D) {
					return mD;
				} else if constexpr (T == E) {
					return mE;
				} else if constexpr (T == F) {
					return mF;
				} else if constexpr (T == H) {
					return mH;
				} else if constexpr (T == L) {
					return mL;
				} else if constexpr (T == AF) {
					return mA << 8 | mF;
				} else if constexpr (T == BC) {
					return mB << 8 | mC;
				} else if constexpr (T == DE) {
					return mD << 8 | mE;
				} else if constexpr (T == HL) {
					return mH << 8 | mL;
				}
			}

			bool getFlagZ() const noexcept { return mF & 0x80; }
			bool getFlagN() const noexcept { return mF & 0x40; }
			bool getFlagH() const noexcept { return mF & 0x20; }
			bool getFlagC() const noexcept { return mF & 0x10; }

			void setFlagZ(const bool state) noexcept { mF = (mF & ~0x8F) | (state << 7); }
			void setFlagN(const bool state) noexcept { mF = (mF & ~0x4F) | (state << 6); }
			void setFlagH(const bool state) noexcept { mF = (mF & ~0x2F) | (state << 5); }
			void setFlagC(const bool state) noexcept { mF = (mF & ~0x1F) | (state << 4); }

			void init_GB() noexcept {
				mA = 0x01; mB = 0x00; mC = 0x13; mD = 0x00;
				mE = 0xD8; mF = 0xB0; mH = 0x01; mL = 0x4D;
			}

			void init_GBC() noexcept {
				mA = 0x11; mB = 0x00; mC = 0x00; mD = 0xFF;
				mE = 0x56; mF = 0x80; mH = 0x00; mL = 0x00;
			}
		} mReg;

		u32  currentPC{};
		u32  stackPtr{};
		bool mIME{};
		bool mMUL{};

		enum class Mode {
			NORMAL, HALT, STOP,
			HALT_BUG, HALT_DI,
			ENABLE_IME,
		} mMode;

	} mCPU;


	struct Opcode {
		using InstrStep = void(*)(CPU* const, MMU* const);

		u32 mOpcode{};
		u32 mLength{};
		u32 mCyclesT{};
		u32 mCyclesM{};
		std::vector<InstrStep>
			mInstrSteps{};

		Opcode(
			u32 opcode, u32 length, u32 cyclesT,
			std::initializer_list<InstrStep> steps
		) noexcept
			: mOpcode{ opcode }
			, mLength{ length }
			, mCyclesT{ cyclesT }
			, mCyclesM{ cyclesT / 4 }
			, mInstrSteps{ steps }
		{}
	};


public:
	GAMEBOY_CLASSIC();

	static constexpr bool validateProgram(
		const char* fileData,
		const size_type   fileSize
	) noexcept {
		if (!fileData || !fileSize) { return false; }
		return false;
	}

	s32 getMaxDisplayW() const noexcept override { return cScreenSizeX; }
	s32 getMaxDisplayH() const noexcept override { return cScreenSizeY; }
};

#endif
