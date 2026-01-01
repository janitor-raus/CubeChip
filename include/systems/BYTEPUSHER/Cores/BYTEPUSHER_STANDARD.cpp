/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BYTEPUSHER_STANDARD.hpp"
#if defined(ENABLE_BYTEPUSHER_SYSTEM) && defined(ENABLE_BYTEPUSHER_STANDARD)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(BYTEPUSHER_STANDARD, ".BytePusher")

/*==================================================================*/

void BYTEPUSHER_STANDARD::initializeSystem() noexcept {
	copyGameToMemory(mMemoryBank.data());

	set_base_system_framerate(cRefreshRate);

	mAudioDevice.add_audio_stream(STREAM::MAIN, u32(get_real_system_framerate() * cAudioLength));
	mAudioDevice.resume_streams();

	mDisplayDevice.metadata_staging
		.set_viewport(cDisplayW, cDisplayH)
		.set_scaling(2).set_padding(4)
		.set_texture_tint(cBitColors[0])
		.enabled = true;
}

/*==================================================================*/

void BYTEPUSHER_STANDARD::instructionLoop() noexcept {
	const auto inputStates = getKeyStates();
	      auto progPointer = readData<3>(2);

	::assign_cast(mMemoryBank[0], inputStates >> 0x8);
	::assign_cast(mMemoryBank[1], inputStates & 0xFF);

	for (auto cycleCount = 0; cycleCount < 0x10000; ++cycleCount) {
		mMemoryBank[readData<3>(progPointer + 3)] =
		mMemoryBank[readData<3>(progPointer + 0)];
		progPointer = readData<3>(progPointer + 6);
	}
}

void BYTEPUSHER_STANDARD::renderAudioData() {
	if (auto* stream = mAudioDevice.at(STREAM::MAIN)) {
		const auto samplesOffset = mMemoryBank.data() + (readData<2>(6) << 8);
		auto buffer = ::allocate_n<f32>(stream->get_next_buffer_size(get_real_system_framerate()))
			.as_value().release_as_container();

		static constexpr auto master_gain = 0.22f;

		std::transform(EXEC_POLICY(unseq)
			samplesOffset, samplesOffset + cAudioLength, buffer.data(),
			[](const auto sample) noexcept { return s8(sample) * (master_gain / 127.0f); }
		);

		mAudioDevice[STREAM::MAIN].push_audio_data(buffer);
	}
}

void BYTEPUSHER_STANDARD::renderVideoData() {
	mDisplayDevice.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = mDisplayDevice.metadata_staging;
		frame.copy_from(mMemoryBank.data() + (readData<1>(5) << 16), cDisplayW * cDisplayH,
			[](u32 pixel) noexcept { return cBitColors[pixel]; }
		);
	});
}

#endif
