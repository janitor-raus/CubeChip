/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDirManager.hpp"
#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"

#include "Chip8_CoreInterface.hpp"

#ifdef ENABLE_CHIP8_SYSTEM

/*==================================================================*/

Chip8_CoreInterface::Chip8_CoreInterface(DisplayDevice display_device) noexcept
	: m_display_device(std::move(display_device))
{
	if (auto* path = HDM->add_user_directory("savestate", "CHIP8")) {
		s_savestate_path = (*path / HDM->get_loaded_file_sha1()).string();
	}

	if (auto* path = HDM->add_user_directory("permaRegs", "CHIP8")) {
		s_permaregs_path = (*path / HDM->get_loaded_file_sha1()).string();
	}

	m_display_device.set_osd_callable([&]() {
		if (m_interrupt == Interrupt::INPUT) {
			osd::key_press_indicator(WaveForms::pulse_t(500, u32(Millis::now())).as_unipolar());
		}
		if (has_system_state(EmuState::STATS)) {
			osd::simple_text_overlay(copy_statistics_string());
		}
	});
	m_display_device.set_shutdown_signal(&m_is_system_alive);

	m_audio_device.add_audio_stream(STREAM::MAIN, 48'000);
	m_audio_device.resume_streams();

	load_preset_binds();
}

/*==================================================================*/

void Chip8_CoreInterface::update_key_states() noexcept {
	if (!m_custom_binds.size()) { return; }

	m_input->update_states();

	m_keys_last = m_keys_this;
	m_keys_this = 0;

	for (const auto& mapping : m_custom_binds) {
		if (m_input->are_any_held(mapping.key, mapping.alt))
			{ m_keys_this |= 1 << mapping.idx; }
	}

	m_keys_loop &= m_keys_hide &= ~(m_keys_last ^ m_keys_this);
}

void Chip8_CoreInterface::load_preset_binds() noexcept {
	static constexpr auto _ = SDL_SCANCODE_UNKNOWN;
	static constexpr SimpleKeyMapping default_key_mappings[]{
		{0x1, KEY(1), _}, {0x2, KEY(2), _}, {0x3, KEY(3), _}, {0xC, KEY(4), _},
		{0x4, KEY(Q), _}, {0x5, KEY(W), _}, {0x6, KEY(E), _}, {0xD, KEY(R), _},
		{0x7, KEY(A), _}, {0x8, KEY(S), _}, {0x9, KEY(D), _}, {0xE, KEY(F), _},
		{0xA, KEY(Z), _}, {0x0, KEY(X), _}, {0xB, KEY(C), _}, {0xF, KEY(V), _},
	};

	load_custom_binds(std::span(default_key_mappings));
}

bool Chip8_CoreInterface::catch_key_press(u8* keyReg) noexcept {
	if (!m_custom_binds.size()) { return false; }

	if (m_elapsed_frames >= m_tick_last + m_tick_span)
		[[unlikely]] { m_keys_last &= ~m_keys_loop; }

	/**/const auto press_keys = m_keys_this & ~m_keys_last;
	if (press_keys) {
		const auto press_diff = press_keys & ~m_keys_loop;
		const auto valid_keys = press_diff ? press_diff : m_keys_loop;

		m_keys_hide |= valid_keys;
		m_tick_last  = m_elapsed_frames;
		m_tick_span  = valid_keys != m_keys_loop ? 20 : 5;
		m_keys_loop  = valid_keys & ~(valid_keys - 1);
		::assign_cast(*keyReg, std::countr_zero(m_keys_loop));
		//m_key_pitch = m_keys_loop ? std::min(m_key_pitch + 8, 80u) : 0;
	}
	return press_keys;
}

bool Chip8_CoreInterface::is_key_held_P1(u32 keyIndex) const noexcept {
	return m_keys_this & ~m_keys_hide & 0x01 << (keyIndex & 0xF);
}

bool Chip8_CoreInterface::is_key_held_P2(u32 keyIndex) const noexcept {
	return m_keys_this & ~m_keys_hide & 0x10 << (keyIndex & 0xF);
}

/*==================================================================*/

void Chip8_CoreInterface::handle_pre_work_interrupts() noexcept {
	switch (m_interrupt)
	{
		case Interrupt::CLEAR:
			return;

		case Interrupt::FRAME:
			m_interrupt = Interrupt::CLEAR;
			return;

		case Interrupt::SOUND:
			for (auto& timer : m_audio_timers)
				{ if (timer.get()) { return; } }
			m_interrupt = Interrupt::WAIT1;
			m_target_cpf = 0;
			return;

		case Interrupt::DELAY:
			if (m_delay_timer) { return; }
			m_interrupt = Interrupt::CLEAR;
			return;

		default:
			return;
	}
}

void Chip8_CoreInterface::handle_post_work_interrupts() noexcept {
	switch (m_interrupt)
	{
		case Interrupt::INPUT:
			if (catch_key_press(m_key_reg_ref)) {
				m_interrupt = Interrupt::CLEAR;
				start_voice_at(VOICE::BUZZER, 2);
			}
			return;

		case Interrupt::WAIT1:
			m_interrupt = Interrupt::FINAL;
			return;

		case Interrupt::ERROR:
			add_system_state(EmuState::FATAL);
			m_target_cpf = 0;
			return;

		case Interrupt::FINAL:
			add_system_state(EmuState::HALTED);
			m_target_cpf = 0;
			return;

		default:
			return;
	}
}

void Chip8_CoreInterface::handle_timer_ticks() noexcept {
	if (m_delay_timer) { --m_delay_timer; }

	for (auto& timer : m_audio_timers)
		{ timer.dec(); }
}

void Chip8_CoreInterface::skip_instruction() noexcept {
	::assign_cast_add(m_current_pc, 2);
}

void Chip8_CoreInterface::jump_program_to(u32 next) noexcept {
	const auto old_pc = (m_current_pc - 2);
	::assign_cast(m_current_pc, next);
	if (m_current_pc == old_pc) [[unlikely]]
		{ trigger_interrupt(Interrupt::SOUND); }
}

/*==================================================================*/

void Chip8_CoreInterface::main_system_loop() {
	update_key_states();

	handle_timer_ticks();
	handle_pre_work_interrupts();
	handle_cycle_loop();
	handle_post_work_interrupts();

	push_audio_data();
	create_statistics_data();
	push_video_data();
}

void Chip8_CoreInterface::append_statistics_data() noexcept {
	if (has_system_state(EmuState::BENCH)) {
		static thread_local ez::EMA suavemente{};

		suavemente.set_alpha(get_real_system_framerate());
		suavemente.add(m_cycle_count * get_real_system_framerate() / 1e6f);

		format_statistics_data(" ::  MIPS:{:8.2f} (frame: {})\n",
			suavemente.avg(), m_benched_frames);
	}

	SystemInterface::append_statistics_data();
}

/*==================================================================*/

void Chip8_CoreInterface::start_voice(u32 duration, u32 tone) noexcept {
	auto voice_index = 0;
	start_voice_at(voice_index, duration, tone);
	if (duration) { ++voice_index %= VOICE::COUNT - 1; }
}

void Chip8_CoreInterface::start_voice_at(u32 voice_index, u32 duration, u32 tone) noexcept {
	m_audio_timers[voice_index].set(duration);
	if (auto* stream = m_audio_device.at(STREAM::MAIN)) {
		m_voices[voice_index].set_step((c_tonal_offset + (tone ? tone : 8 \
			* (((m_current_pc >> 1) + m_stack_head + 1) & 0x3E) \
		)) / stream->get_freq() * get_framerate_multiplier());
	}
}

void Chip8_CoreInterface::mix_audio_data(VoiceGenerators processors) noexcept {
	if (auto* stream = m_audio_device.at(STREAM::MAIN)) {

		auto buffer = ::allocate_n<f32>
			(stream->get_next_buffer_size(get_real_system_framerate()))
			.as_value().release_as_container();

		for (auto& bundle : processors)
			{ bundle.run(buffer, stream); }

		for (auto& sample : buffer)
			{ sample = ez::fast_tanh(sample); }

		stream->push_audio_data(buffer);
	}
}

void Chip8_CoreInterface::make_pulse_wave(f32* data, u32 size, Voice* voice, Stream*) noexcept {
	if (!voice || !voice->userdata) [[unlikely]] { return; }
	auto* timer = static_cast<AudioTimer*>(voice->userdata);

	for (auto i{ 0u }; i < size; ++i) {
		if (const auto gain = voice->get_level(i, *timer)) {
			::assign_cast_add(data[i], \
				WaveForms::pulse(voice->peek_phase(i)) * gain);
		} else break;
	}
	voice->step_phase(size);
}

void Chip8_CoreInterface::instruction_error(u32 HI, u32 LO) noexcept {
	blog.info("Unknown instruction: 0x{:04X}", (HI << 8) | LO);
	trigger_interrupt(Interrupt::ERROR);
}

void Chip8_CoreInterface::trigger_interrupt(Interrupt type) noexcept {
	set_frame_stop_flag(true);
	m_interrupt = type;
}

void Chip8_CoreInterface::trigger_interrupt(Interrupt type, bool cond) noexcept {
	if (cond) [[unlikely]] { trigger_interrupt(type); }
}

/*==================================================================*/

static bool has_regular_file(std::string_view file_path) noexcept {
	const auto is_regular = fs::is_regular_file(file_path);
	if (!is_regular) {
		blog.debug("\"{}\" [{}]", file_path, is_regular.error().message());
		return false;
	}
	return is_regular.value();
}

static bool make_permaregs_file(std::string_view file_path) noexcept {
	static constexpr char data_padding[16]{};
	const auto is_created = ::write_file_data(file_path, data_padding);
	if (!is_created) {
		blog.error("\"{}\" [{}]", file_path, is_created.error().message());
	}
	return is_created.value();
}

void Chip8_CoreInterface::set_file_permaregs(u32 X) noexcept {
	auto write_status = ::write_file_data(s_permaregs_path, m_registers_V, X);
	if (!write_status) {
		blog.error("File IO error: \"{}\" [{}]",
			s_permaregs_path, write_status.error().message());
	}
}

void Chip8_CoreInterface::get_file_permaregs(u32 X) noexcept {
	::assign_cast_min(X, s_permaregs_V.size());
	auto read_status = ::read_file_data(s_permaregs_path, X);
	if (!read_status) {
		blog.error("File IO error: \"{}\" [{}]",
			s_permaregs_path, read_status.error().message());
	} else {
		std::copy_n(read_status.value().begin(), X, s_permaregs_V.begin());
	}
}

void Chip8_CoreInterface::set_permaregs(u32 X) noexcept {
	if (!s_permaregs_path.empty()) {
		if (has_regular_file(s_permaregs_path)) { set_file_permaregs(X); }
		else { s_permaregs_path.clear(); }
	}
	std::copy_n(m_registers_V.begin(), X, s_permaregs_V.begin());
}

void Chip8_CoreInterface::get_permaregs(u32 X) noexcept {
	if (!s_permaregs_path.empty()) {
		if (!has_regular_file(s_permaregs_path)) {
			if (!make_permaregs_file(s_permaregs_path)) { s_permaregs_path.clear(); }
		}

		if (has_regular_file(s_permaregs_path)) { get_file_permaregs(X); }
		else { s_permaregs_path.clear(); }
	}
	std::copy_n(s_permaregs_V.begin(), X, m_registers_V.begin());
}

/*==================================================================*/

void Chip8_CoreInterface::copy_game_to_memory(void* dest) noexcept {
	std::memcpy(dest, HDM->get_loaded_file_data(), HDM->get_loaded_file_size());
}

void Chip8_CoreInterface::copy_font_to_memory(void* dest, size_type size) noexcept {
	std::memcpy(dest, std::data(s_fonts_data), size);
}

/*==================================================================*/

#endif
