/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BYTEPUSHER_STANDARD.hpp"
#if defined(ENABLE_BYTEPUSHER_SYSTEM) && defined(ENABLE_BYTEPUSHER_STANDARD)

#include "AssignCast.hpp"
#include "CoreRegistry.inl"

REGISTER_SYSTEM_CORE(BYTEPUSHER_STANDARD)

/*==================================================================*/

void BYTEPUSHER_STANDARD::initialize_system() noexcept {
	copy_file_image_to(m_memory, 0);

	m_base_system_framerate = c_sys_refresh_rate;

	m_audio_device.init_stream(s32(c_sys_refresh_rate * c_sys_audio_sample_total), 1);
	m_audio_device.resume();

	m_memory_editor.set_memory_range(m_memory.data(), m_memory.size());

	m_display_device.metadata().edit([](auto& meta) noexcept {
		meta.minimum_zoom = 2;
		meta.inner_margin = 4;
		meta.texture_tint = c_bit_colors[0];
		meta.enabled = true;
	});
}

/*==================================================================*/

void BYTEPUSHER_STANDARD::handle_cycle_loop() noexcept {
	const auto input_states = get_key_states();
	/***/ auto prog_pointer = get_program_counter();

	::assign_cast(m_memory[0], input_states >> 0x8);
	::assign_cast(m_memory[1], input_states & 0xFF);

	for (auto cycle_count = 0; cycle_count < c_sys_standard_cpf; ++cycle_count) {
		m_memory[read_data<ByteSpan::TRIPLE>(prog_pointer + 3)] =
		m_memory[read_data<ByteSpan::TRIPLE>(prog_pointer + 0)];
		prog_pointer = read_data<ByteSpan::TRIPLE>(prog_pointer + 6);
	}
}

void BYTEPUSHER_STANDARD::push_audio_data() noexcept {
	if (m_audio_device) {
		m_audio_device.set_freq_ratio(m_framerate_multiplier);

		float buffer[c_sys_audio_sample_total]{};

		if (!has_cached_system_state(EmuState::ANY_PAUSE)) {
			static constexpr auto c_master_gain = 0.5f;

			const auto samples = std::span(m_memory.data()
				+ (read_data<ByteSpan::DOUBLE>(6) << 8), 256);

			std::transform(EXEC_POLICY(unseq)
				samples.begin(), samples.end(), buffer,
				[](const auto sample) noexcept {
					return s8(sample) * (c_master_gain / 127.0f);
				}
			);
		}

		m_audio_device.push_audio_data(buffer);
	}
}

void BYTEPUSHER_STANDARD::push_video_data() noexcept {
	m_display_device.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = m_display_device.metadata().copy();
		frame.copy_from(&m_memory[read_data<ByteSpan::SINGLE>(5) << 16],
			c_sys_screen_W * c_sys_screen_H,
			[](const auto pixel) noexcept { return c_bit_colors[pixel]; }
		);
	});
}

#endif
