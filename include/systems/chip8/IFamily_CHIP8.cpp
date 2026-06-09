/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "IFamily_CHIP8.hpp"

#ifdef ENABLE_CHIP8_SYSTEM

#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"
#include "SimpleTimer.hpp"

/*==================================================================*/

IFamily_CHIP8::IFamily_CHIP8(std::size_t W, std::size_t H) noexcept
	: ISystemEmu(family_pretty_name)
	, m_display_window({ "Display", make_system_id(instance_id, "display") })
	, m_display_device(W, H, UserInterface::get_current_renderer())
{
	prepare_user_interface();
	load_preset_binds();

	m_audio_device.init_stream(0, 1);
	m_audio_device.resume();
}

void IFamily_CHIP8::initialize_family() noexcept {
	if (calc_file_image_sha1()) {
		if (auto* path = add_system_path("savestate", family_name)) {
			m_savestate_path = (fs::Path(*path) / m_file_sha1_hash).string();
		} else {
			blog.error("Unable to create savestate directory for system '{}', "
				"savestates will be unavailable!", family_pretty_name);
		}

		if (auto* path = add_system_path("permaregs", family_name)) {
			m_permaregs_path = (fs::Path(*path) / m_file_sha1_hash).string();
		} else {
			blog.warn("Unable to create permaregs directory for system '{}', "
				"permanent register storage will be unavailable!", family_pretty_name);
		}
	}
}

/*==================================================================*/

void IFamily_CHIP8::update_key_states() noexcept {
	if (!m_custom_binds.size()) { return; }

	m_input.advance_state();

	m_keys_last = m_keys_this;
	m_keys_this = 0;

	for (const auto& mapping : m_custom_binds) {
		if (m_input.is_held(mapping.key) || m_input.is_held(mapping.alt)) {
			m_keys_this |= 1 << mapping.idx;
		}
	}

	m_keys_loop &= m_keys_hide &= ~(m_keys_last ^ m_keys_this);
}

void IFamily_CHIP8::load_preset_binds() noexcept {
	static constexpr auto _ = SDL_SCANCODE_UNKNOWN;
	static constexpr SimpleKeyMapping default_key_mappings[]{
		{0x1, KEY(1), _}, {0x2, KEY(2), _}, {0x3, KEY(3), _}, {0xC, KEY(4), _},
		{0x4, KEY(Q), _}, {0x5, KEY(W), _}, {0x6, KEY(E), _}, {0xD, KEY(R), _},
		{0x7, KEY(A), _}, {0x8, KEY(S), _}, {0x9, KEY(D), _}, {0xE, KEY(F), _},
		{0xA, KEY(Z), _}, {0x0, KEY(X), _}, {0xB, KEY(C), _}, {0xF, KEY(V), _},
	};

	load_custom_binds(std::span(default_key_mappings));
}

bool IFamily_CHIP8::catch_key_press(u8* key_reg) noexcept {
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
		::assign_cast(*key_reg, std::countr_zero(m_keys_loop));
		//m_key_pitch = m_keys_loop ? std::min(m_key_pitch + 8, 80u) : 0;
	}
	return press_keys;
}

bool IFamily_CHIP8::is_key_held_P1(u32 key_index) const noexcept {
	return m_keys_this & ~m_keys_hide & (0x01 << (key_index & 0xF));
}

bool IFamily_CHIP8::is_key_held_P2(u32 key_index) const noexcept {
	return m_keys_this & ~m_keys_hide & (0x10 << (key_index & 0xF));
}

/*==================================================================*/

void IFamily_CHIP8::handle_pre_work_interrupts() noexcept {
	switch (m_interrupt)
	{
		case Interrupt::FRAME:
			m_interrupt = Interrupt::CLEAR;
			return;

		case Interrupt::SOUND:
			for (auto& voice : m_voices) {
				if (voice.timer.get()) { return; }
			}
			m_interrupt = Interrupt::WAIT1;
			m_standard_cpf = 0;
			return;

		case Interrupt::DELAY:
			if (m_delay_timer) { return; }
			m_interrupt = Interrupt::CLEAR;
			return;

		case Interrupt::CLEAR:
		default:
			return;
	}
}

void IFamily_CHIP8::handle_post_work_interrupts() noexcept {
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
			m_standard_cpf = 0;
			return;

		case Interrupt::FINAL:
			add_system_state(EmuState::HALTED);
			m_standard_cpf = 0;
			return;

		default:
			return;
	}
}

void IFamily_CHIP8::handle_timer_ticks() noexcept {
	if (m_delay_timer) { --m_delay_timer; }

	for (auto& voice : m_voices)
		{ voice.timer.dec(); }
}

void IFamily_CHIP8::skip_instruction() noexcept {
	::assign_cast_add(m_current_pc, 2);
}

void IFamily_CHIP8::jump_program_to(u32 next) noexcept {
	const auto old_pc = (m_current_pc - 2);
	::assign_cast(m_current_pc, next);
	if (m_current_pc == old_pc) [[unlikely]]
		{ trigger_interrupt(Interrupt::SOUND); }
}

void IFamily_CHIP8::handle_cycle_loop() noexcept {
	if (has_cached_system_state(EmuState::BENCH)) {
		// avg time multiplier to cushion against slice jitter
		static constexpr auto c_jitter_multiplier = 1.1f;

		ez::EMA frametime_ema;
		SimpleTimer slice_timer;
		u32 total_cycles = 0;

		frametime_ema.set_alpha(get_real_system_framerate());
		slice_timer.start();

		do {
			instruction_loop();
			total_cycles += m_cycle_count;
			if (m_interrupt != Interrupt::CLEAR) { break; }
			frametime_ema.add(slice_timer.lap_millis());
		}
		while (frametime_ema.avg() * c_jitter_multiplier < m_pacer.get_period_remaining());
		m_cycle_count = total_cycles;
		return;
	} else {
		instruction_loop();
	}
}

/*==================================================================*/

void IFamily_CHIP8::main_system_loop() {
	// cache quirk flags every frame start
	m_cached_quirk_flags = m_quirk_flags;

	if (has_cached_system_state(EmuState::ANY_PAUSE)) {
		push_audio_data();
		return;
	}

	update_key_states();

	handle_timer_ticks();
	handle_pre_work_interrupts();
	handle_cycle_loop();
	handle_post_work_interrupts();

	push_audio_data();
	push_video_data();
	create_statistics_data();
}

void IFamily_CHIP8::append_statistics_data() noexcept {
	if (has_cached_system_state(EmuState::BENCH)) {
		auto& mips_ema = m_mips_ema; // msvc likes this

		mips_ema.set_alpha(get_real_system_framerate());
		mips_ema.add(m_cycle_count * get_real_system_framerate() / 1e6f);

		format_statistics_data("> MIPS:{:8.2f} avg over {} frames\n",
			mips_ema.avg(), m_benched_frames);
	}

	ISystemEmu::append_statistics_data();
}

/*==================================================================*/

void IFamily_CHIP8::start_voice(u32 duration) noexcept {
	start_voice_at(m_last_voice_index, duration);
	if (duration) { ++m_last_voice_index %= VOICE::COUNT - 1; }
}

void IFamily_CHIP8::start_voice_at(u32 voice_index, u32 duration) noexcept {
	m_voices[voice_index].timer.set(duration);
	if (m_audio_device) {
		m_voices[voice_index].set_step((c_tonal_offset + 8 *
			(((m_current_pc >> 1) + m_stack.head() + 1) & 0x3E)
		) / m_audio_device.get_freq());
	}
}

void IFamily_CHIP8::make_pulse_wave(SampleBuffer buffer, Voice& voice) noexcept {
	if (const auto sample_count = u32(buffer.size())) {
		const auto fade_step = ::calc_fade_step(sample_count);

		for (auto i = 0u; i < sample_count; ++i) {
			if (const auto gain = voice.get_level(i, voice.timer, fade_step)) {
				::assign_cast_add(buffer[i], \
					WaveForms::pulse(voice.peek_phase(i)) * gain);
			}
			else break;
		}
		voice.step_phase(sample_count);
	}
}

void IFamily_CHIP8::instruction_error(u32 HI, u32 LO) noexcept {
	blog.info("Unknown instruction: 0x{:04X}", (HI << 8) | LO);
	trigger_interrupt(Interrupt::ERROR);
}

/*==================================================================*/

static bool has_regular_file(std::string_view file_path) noexcept {
	const auto is_regular = fs::is_regular_file(file_path);
	if (!is_regular) {
		blog.debug("'{}': {}", file_path, is_regular.error().message());
		return false;
	}
	return is_regular.value();
}

static bool make_permaregs_file(std::string_view file_path) noexcept {
	static constexpr char data_padding[16]{};
	const auto is_created = ::write_file_data(file_path, data_padding);
	if (!is_created) {
		blog.error("'{}': {}", file_path, is_created.error().message());
	}
	return is_created.value();
}

void IFamily_CHIP8::set_file_permaregs(u32 X) noexcept {
	auto write_status = ::write_file_data(m_permaregs_path, m_registers_V, X);
	if (!write_status) {
		blog.error("File IO error '{}': {}",
			m_permaregs_path, write_status.error().message());
	}
}

void IFamily_CHIP8::get_file_permaregs(u32 X) noexcept {
	::assign_cast_min(X, s_permaregs_V.size());
	auto read_status = ::read_file_data(m_permaregs_path, X);
	if (!read_status) {
		blog.error("File IO error: '{}': {}",
			m_permaregs_path, read_status.error().message());
	} else {
		std::copy_n(read_status.value().begin(), X, s_permaregs_V.begin());
	}
}

void IFamily_CHIP8::set_permaregs(u32 X) noexcept {
	if (!m_permaregs_path.empty()) {
		if (has_regular_file(m_permaregs_path)) { set_file_permaregs(X); }
		else { m_permaregs_path.clear(); }
	}
	std::copy_n(m_registers_V.begin(), X, s_permaregs_V.begin());
}

void IFamily_CHIP8::get_permaregs(u32 X) noexcept {
	if (!m_permaregs_path.empty()) {
		if (!has_regular_file(m_permaregs_path)) {
			if (!make_permaregs_file(m_permaregs_path)) { m_permaregs_path.clear(); }
		}

		if (has_regular_file(m_permaregs_path)) { get_file_permaregs(X); }
		else { m_permaregs_path.clear(); }
	}
	std::copy_n(s_permaregs_V.begin(), X, m_registers_V.begin());
}

/*==================================================================*/

void IFamily_CHIP8::copy_font_data_to(std::span<u8> dest, std::size_t size) noexcept {
	std::memcpy(dest.data() + c_small_font_offset, s_fonts_data.data(),
		std::min(size, s_fonts_data.size() - c_small_font_offset));
}

/*==================================================================*/

#endif
