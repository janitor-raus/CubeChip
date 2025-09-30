/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDirManager.hpp"
#include "BasicVideoSpec.hpp"
#include "GlobalAudioBase.hpp"
#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"

#include "Chip8_CoreInterface.hpp"

#ifdef ENABLE_CHIP8_SYSTEM

/*==================================================================*/

Chip8_CoreInterface::Chip8_CoreInterface() noexcept {
	if (const auto* path{ HDM->addSystemDir("savestate", "CHIP8") })
		{ sSavestatePath = *path / HDM->getFileSHA1(); }

	if (const auto* path{ HDM->addSystemDir("permaRegs", "CHIP8") })
		{ sPermaRegsPath = *path / HDM->getFileSHA1(); }

	mAudioDevice.addAudioStream(STREAM::MAIN, 48'000);
	mAudioDevice.resumeStreams();

	loadPresetBinds();
}

/*==================================================================*/

void Chip8_CoreInterface::updateKeyStates() {
	if (!std::size(mCustomBinds)) { return; }

	Input->updateStates();

	mKeysPrev = mKeysCurr;
	mKeysCurr = 0;

	for (const auto& mapping : mCustomBinds) {
		if (Input->areAnyHeld(mapping.key, mapping.alt))
			{ mKeysCurr |= 1 << mapping.idx; }
	}

	mKeysLoop &= mKeysLock &= ~(mKeysPrev ^ mKeysCurr);
}

void Chip8_CoreInterface::loadPresetBinds() {
	static constexpr auto _{ SDL_SCANCODE_UNKNOWN };
	static constexpr SimpleKeyMapping defaultKeyMappings[]{
		{0x1, KEY(1), _}, {0x2, KEY(2), _}, {0x3, KEY(3), _}, {0xC, KEY(4), _},
		{0x4, KEY(Q), _}, {0x5, KEY(W), _}, {0x6, KEY(E), _}, {0xD, KEY(R), _},
		{0x7, KEY(A), _}, {0x8, KEY(S), _}, {0x9, KEY(D), _}, {0xE, KEY(F), _},
		{0xA, KEY(Z), _}, {0x0, KEY(X), _}, {0xB, KEY(C), _}, {0xF, KEY(V), _},
	};

	loadCustomBinds(std::span(defaultKeyMappings));
}

bool Chip8_CoreInterface::keyPressed(u8* keyReg) noexcept {
	if (!std::size(mCustomBinds)) { return false; }

	const auto mTickCurr{ u32(Pacer->getValidFrameCounter()) };
	if (mTickCurr >= mTickLast + mTickSpan)
		[[unlikely]] { mKeysPrev &= ~mKeysLoop; }

	/**/const auto pressKeys{ mKeysCurr & ~mKeysPrev };
	if (pressKeys) {
		const auto pressDiff{ pressKeys & ~mKeysLoop };
		const auto validKeys{ pressDiff ? pressDiff : mKeysLoop };

		mKeysLock |= validKeys;
		mTickLast  = mTickCurr;
		mTickSpan  = validKeys != mKeysLoop ? 20 : 5;
		mKeysLoop  = validKeys & ~(validKeys - 1);
		::assign_cast(*keyReg, std::countr_zero(mKeysLoop));
		//mKeyPitch = mKeysLoop ? std::min(mKeyPitch + 8, 80u) : 0;
	}
	return pressKeys;
}

bool Chip8_CoreInterface::keyHeld_P1(u32 keyIndex) const noexcept {
	return mKeysCurr & ~mKeysLock & 0x01 << (keyIndex & 0xF);
}

bool Chip8_CoreInterface::keyHeld_P2(u32 keyIndex) const noexcept {
	return mKeysCurr & ~mKeysLock & 0x10 << (keyIndex & 0xF);
}

/*==================================================================*/

void Chip8_CoreInterface::handlePreFrameInterrupt() noexcept {
	switch (mInterrupt)
	{
		case Interrupt::CLEAR:
			mTargetCPF *= mTargetCPF < 0 ? -1 : 1;
			return;

		case Interrupt::FRAME:
			mInterrupt = Interrupt::CLEAR;
			mTargetCPF *= mTargetCPF < 0 ? -1 : 1;
			return;

		case Interrupt::SOUND:
			for (auto& timer : mAudioTimers)
				{ if (timer.get()) { return; } }
			mInterrupt = Interrupt::WAIT1;
			mTargetCPF = 0;
			return;

		case Interrupt::DELAY:
			if (mDelayTimer) { return; }
			mInterrupt = Interrupt::CLEAR;
			mTargetCPF *= mTargetCPF < 0 ? -1 : 1;
			return;

		default:
			break;
	}
}

void Chip8_CoreInterface::handleEndFrameInterrupt() noexcept {
	switch (mInterrupt)
	{
		case Interrupt::INPUT:
			if (keyPressed(mInputReg)) {
				mInterrupt = Interrupt::CLEAR;
				mTargetCPF *= mTargetCPF < 0 ? -1 : 1;
				startVoiceAt(VOICE::BUZZER, 2);
			}
			return;

		case Interrupt::WAIT1:
			mInterrupt = Interrupt::FINAL;
			return;

		case Interrupt::ERROR:
			addSystemState(EmuState::FATAL);
			mTargetCPF = 0;
			return;

		case Interrupt::FINAL:
			setSystemState(EmuState::HALTED);
			mTargetCPF = 0;
			return;

		default:
			break;
	}
}

void Chip8_CoreInterface::handleTimerTick() noexcept {
	if (mDelayTimer) { --mDelayTimer; }

	for (auto& timer : mAudioTimers)
		{ timer.dec(); }
}

void Chip8_CoreInterface::nextInstruction() noexcept {
	mCurrentPC += 2;
}

void Chip8_CoreInterface::skipInstruction() noexcept {
	mCurrentPC += 2;
}

void Chip8_CoreInterface::performProgJump(u32 next) noexcept {
	const auto oldPC{ mCurrentPC - 2u };
	::assign_cast(mCurrentPC, next & 0xFFFu);
	if (mCurrentPC == oldPC) [[unlikely]]
		{ triggerInterrupt(Interrupt::SOUND); }
}

/*==================================================================*/

void Chip8_CoreInterface::mainSystemLoop() {
	if (!isSystemRunning())
		[[unlikely]] { return; }

	updateKeyStates();

	handleTimerTick();
	handlePreFrameInterrupt();
	instructionLoop();
	handleEndFrameInterrupt();

	renderAudioData();
	renderVideoData();
	pushOverlayData();
}

Str* Chip8_CoreInterface::makeOverlayData() {
	static constexpr std::array<f32, 5> stride
		{ { 1.5e6f, 1.0e6f, 0.6e6f, 0.3e6f, 1.0e5f } };

	const auto frameTime{ Pacer->getElapsedMicrosSince() / 1000.0f };
	const auto timePhase{ std::min(frameTime * 1.030f / Pacer->getFramespan(), 2.0f) };
	const auto newStride{ ez::peak_mirror_fold(u32(timePhase / 0.2f), stride.size()) };
	const auto cycleStep{ stride[newStride] * std::cos(timePhase * f32(std::numbers::pi / 2)) };

	if (mInterrupt == Interrupt::CLEAR) [[likely]]
		{ ::assign_cast_add(mTargetCPF, cycleStep); }

	*getOverlayDataBuffer() = fmt::format(
		" ::  MIPS:{:8.2f}\n{}",
		mTargetCPF * getRealSystemFramerate() / 1'000'000.0f,
		*SystemInterface::makeOverlayData()
	);
	return getOverlayDataBuffer();
}

void Chip8_CoreInterface::pushOverlayData() {
	if (getSystemState() & EmuState::BENCH) [[likely]]
		{ saveOverlayData(makeOverlayData()); }
	else { SystemInterface::pushOverlayData(); }
}

/*==================================================================*/

void Chip8_CoreInterface::startVoice(s32 duration, s32 tone) noexcept {
	thread_local auto voice_index{ 0 };
	startVoiceAt(voice_index, duration, tone);
	if (duration) { ++voice_index %= VOICE::COUNT - 1; }
}

void Chip8_CoreInterface::startVoiceAt(u32 voice_index, u32 duration, u32 tone) noexcept {
	mAudioTimers[voice_index].set(duration);
	if (auto* stream{ mAudioDevice.at(STREAM::MAIN) }) {
		mVoices[voice_index].setStep((sTonalOffset + (tone ? tone : 8 \
			* (((mCurrentPC >> 1) + mStackTop + 1) & 0x3E) \
		)) / stream->getFreq() * getFramerateMultiplier());
	}
}

void Chip8_CoreInterface::mixAudioData(VoiceGenerators processors) noexcept {
	if (auto* stream{ mAudioDevice.at(STREAM::MAIN) }) {

		auto buffer{ ::allocate_n<f32>(stream->getNextBufferSize(getRealSystemFramerate()))
			.as_value().release_as_container() };

		for (auto& bundle : processors)
			{ bundle.run(buffer, stream); }

		for (auto& sample : buffer)
			{ sample = ez::fast_tanh(sample); }

		stream->pushAudioData(buffer);
	}
}

void Chip8_CoreInterface::makePulseWave(f32* data, u32 size, Voice* voice, Stream*) noexcept {
	if (!voice || !voice->userdata) [[unlikely]] { return; }
	auto* timer{ static_cast<AudioTimer*>(voice->userdata) };

	for (auto i{ 0u }; i < size; ++i) {
		if (const auto gain{ voice->getLevel(i, *timer) }) {
			::assign_cast_add(data[i], \
				WaveForms::pulse(voice->peekPhase(i)) * gain);
		} else break;
	}
	voice->stepPhase(size);
}

void Chip8_CoreInterface::instructionError(u32 HI, u32 LO) {
	blog.newEntry<BLOG::INFO>("Unknown instruction: 0x{:02X}{:02X}", HI, LO);
	triggerInterrupt(Interrupt::ERROR);
}

void Chip8_CoreInterface::triggerInterrupt(Interrupt type) noexcept {
	mInterrupt = type;
	mTargetCPF = mTargetCPF < 0 ? mTargetCPF : -mTargetCPF;
}

/*==================================================================*/

bool Chip8_CoreInterface::checkRegularFile(const Path& filePath) const noexcept {
	const auto fileRegular{ fs::is_regular_file(filePath) };
	if (!fileRegular) {
		blog.newEntry<BLOG::ERROR>("\"{}\" [{}]",
			filePath.string(), fileRegular.error().message());
		return false;
	}
	return fileRegular.value();
}

bool Chip8_CoreInterface::newPermaRegsFile(const Path& filePath) const noexcept {
	static constexpr char dataPadding[std::size(sPermRegsV)]{};
	const auto fileCreated{ ::writeFileData(filePath, dataPadding) };
	if (!fileCreated) {
		blog.newEntry<BLOG::ERROR>("\"{}\" [{}]",
			filePath.string(), fileCreated.error().message());
	}
	return fileCreated.value();
}

void Chip8_CoreInterface::setFilePermaRegs(u32 X) noexcept {
	auto fileData{ ::writeFileData(sPermaRegsPath, mRegisterV, X) };
	if (!fileData) {
		blog.newEntry<BLOG::ERROR>("File IO error: \"{}\" [{}]",
			sPermaRegsPath.string(), fileData.error().message());
	}
}

void Chip8_CoreInterface::getFilePermaRegs(u32 X) noexcept {
	::assign_cast(X, std::min(X, u32(sPermRegsV.size())));
	auto fileData{ ::readFileData(sPermaRegsPath, X) };
	if (!fileData) {
		blog.newEntry<BLOG::ERROR>("File IO error: \"{}\" [{}]",
			sPermaRegsPath.string(), fileData.error().message());
	} else {
		std::copy_n(fileData.value().begin(), X, sPermRegsV.begin());
	}
}

void Chip8_CoreInterface::setPermaRegs(u32 X) noexcept {
	if (!sPermaRegsPath.empty()) {
		if (checkRegularFile(sPermaRegsPath)) { setFilePermaRegs(X); }
		else { sPermaRegsPath.clear(); }
	}
	std::copy_n(mRegisterV.begin(), X, sPermRegsV.begin());
}

void Chip8_CoreInterface::getPermaRegs(u32 X) noexcept {
	if (!sPermaRegsPath.empty()) {
		if (!checkRegularFile(sPermaRegsPath)) {
			if (!newPermaRegsFile(sPermaRegsPath)) { sPermaRegsPath.clear(); }
		}

		if (checkRegularFile(sPermaRegsPath)) { getFilePermaRegs(X); }
		else { sPermaRegsPath.clear(); }
	}
	std::copy_n(sPermRegsV.begin(), X, mRegisterV.begin());
}

/*==================================================================*/

void Chip8_CoreInterface::copyGameToMemory(void* dest) noexcept {
	std::memcpy(dest, HDM->getFileData(), HDM->getFileSize());
}

void Chip8_CoreInterface::copyFontToMemory(void* dest, size_type size) noexcept {
	std::memcpy(dest, std::data(sFontsData), size);
}

void Chip8_CoreInterface::copyColorsToCore(void* dest) noexcept {
	std::memcpy(dest, std::data(sBitColors), std::size(sBitColors) * sizeof(decltype(sBitColors)::value_type));
}

/*==================================================================*/

#endif
