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

void BYTEPUSHER_STANDARD::initialize_system() noexcept {
	copy_game_to_memory(m_memory_bank.data());

	set_base_system_framerate(c_sys_refresh_rate);

	m_audio_device.add_audio_stream(STREAM::MAIN, u32(get_real_system_framerate() * cAudioLength));
	m_audio_device.resume_streams();

	m_display_device.metadata_staging()
		.set_viewport(c_sys_screen_W, c_sys_screen_H)
		.set_minimum_zoom(2).set_inner_margin(4)
		.set_texture_tint(c_bit_colors[0])
		.enabled = true;
}

/*==================================================================*/

void BYTEPUSHER_STANDARD::instruction_loop() noexcept {
	const auto inputStates = get_key_states();
	      auto progPointer = readData<3>(2);

	::assign_cast(m_memory_bank[0], inputStates >> 0x8);
	::assign_cast(m_memory_bank[1], inputStates & 0xFF);

	for (auto cycleCount = 0; cycleCount < 0x10000; ++cycleCount) {
		m_memory_bank[readData<3>(progPointer + 3)] =
		m_memory_bank[readData<3>(progPointer + 0)];
		progPointer = readData<3>(progPointer + 6);
	}
}

void BYTEPUSHER_STANDARD::push_audio_data() {
	if (auto* stream = m_audio_device.at(STREAM::MAIN)) {
		const auto samplesOffset = m_memory_bank.data() + (readData<2>(6) << 8);
		auto buffer = ::allocate_n<f32>(stream->get_next_buffer_size(get_real_system_framerate()))
			.as_value().release_as_container();

		static constexpr auto master_gain = 0.22f;

		std::transform(EXEC_POLICY(unseq)
			samplesOffset, samplesOffset + cAudioLength, buffer.data(),
			[](const auto sample) noexcept { return s8(sample) * (master_gain / 127.0f); }
		);

		m_audio_device[STREAM::MAIN].push_audio_data(buffer);
	}
}

void BYTEPUSHER_STANDARD::push_video_data() {
	m_display_device.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = ++m_display_device.metadata_staging();
		frame.copy_from(m_memory_bank.data() + (readData<1>(5) << 16), c_sys_screen_W * c_sys_screen_H,
			[](u32 pixel) noexcept { return c_bit_colors[pixel]; }
		);
	});
}

#endif
