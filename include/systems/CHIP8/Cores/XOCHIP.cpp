/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "XOCHIP.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_XOCHIP)

#include "CoreRegistry.hpp"

REGISTER_CORE(XOCHIP, ".xo8")

/*==================================================================*/

void XOCHIP::initialize_system() noexcept {
	Quirk.wrap_sprites = true;

	copy_game_to_memory(m_memory_bank.data() + c_game_load_pos);
	copy_font_to_memory(m_memory_bank.data(), 80);

	set_base_system_framerate(c_sys_refresh_rate);

	set_pattern_pitch(64);

	m_voices[VOICE::UNIQUE].userdata = &m_audio_timers[VOICE::UNIQUE];
	m_voices[VOICE::BUZZER].userdata = &m_audio_timers[VOICE::BUZZER];

	m_current_pc = c_sys_boot_pos;
	m_target_cpf = c_sys_speed_lo;
	m_bit_colors = s_bit_colors;

	auto& meta = m_display_device.metadata_staging();

	meta.minimum_zoom = 4;
	meta.inner_margin = 4;
	meta.texture_tint = m_bit_colors[0];
	meta.enabled = true;
}

void XOCHIP::handle_cycle_loop() noexcept
	{ LOOP_DISPATCH(instruction_loop); }

template <typename Lambda>
void XOCHIP::instruction_loop(Lambda&& condition) noexcept {
	for (m_cycle_count = 0; condition(); ++m_cycle_count) {
		const auto HI = m_memory_bank[m_current_pc++];
		const auto LO = m_memory_bank[m_current_pc++];

		#define _NNN ((HI << 8 | LO) & 0xFFF)
		#define _X (HI & 0xF)
		#define Y_ (LO >> 4)
		#define _N (LO & 0xF)

		switch (HI) {
			case 0x00:
				switch (LO) {
					CASE_xNF(0xC0):
						instruction_00CN(_N);
						break;
					CASE_xNF(0xD0):
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
					default: instruction_error(HI, LO);
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
						instruction_error(HI, LO);
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
					default: instruction_error(HI, LO);
				}
				break;
			CASE_xNF(0x90):
				if (_N) [[unlikely]] {
					instruction_error(HI, LO);
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
					default: instruction_error(HI, LO);
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
					default: instruction_error(HI, LO);
				}
				break;
		}
	}
}

void XOCHIP::push_audio_data() noexcept {
	mix_audio_data({
		{ make_pattern_wave, &m_voices[VOICE::UNIQUE] },
		{ make_pulse_wave,   &m_voices[VOICE::BUZZER] },
	});

	m_display_device.metadata_staging().set_border_color_if(
		!!m_audio_timers[VOICE::BUZZER], m_bit_colors[1]);
}

void XOCHIP::push_video_data() noexcept {
	std::array<u8, c_sys_screen_W * c_sys_screen_H> composite_buffer{};

	const auto merge_bit_color = [&](std::size_t idx) noexcept {
		return m_display_map[P0](idx) << 0 |
			   m_display_map[P1](idx) << 1 |
			   m_display_map[P2](idx) << 2 |
			   m_display_map[P3](idx) << 3 ;
	};

	if (use_hires_screen()) {
		std::for_each(EXEC_POLICY(unseq)
			composite_buffer.begin(), composite_buffer.end(),
			[&](auto& pixel) noexcept {
				::assign_cast(pixel, merge_bit_color
				(&pixel - composite_buffer.data()));
			}
		);
	} else {
		std::for_each(EXEC_POLICY(unseq)
			composite_buffer.begin(), composite_buffer.end(),
			[&](auto& pixel) noexcept {
				const auto idx = &pixel - composite_buffer.data();

				const auto x = idx % c_sys_screen_W;
				const auto y = idx / c_sys_screen_W;

				::assign_cast(pixel, merge_bit_color
					((y/2) * c_sys_screen_H + (x/2)));
			}
		);
	}

	m_display_device.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = ++m_display_device.metadata_staging();
		frame.copy_from(composite_buffer, [&](auto pixel) noexcept { return m_bit_colors[pixel]; });
	});
}

void XOCHIP::set_pattern_pitch(s32 pitch) noexcept {
	if (auto* stream = m_audio_device.at(STREAM::MAIN)) {
		m_voices[VOICE::UNIQUE].set_step(std::bit_cast<f32>(
			c_pitch_frequency_lut[pitch]) / stream->get_freq() * get_framerate_multiplier());
	}
}

void XOCHIP::make_pattern_wave(f32* data, u32 size, Voice* voice, Stream*) noexcept {
	if (!voice || !voice->userdata) [[unlikely]] { return; }
	auto* timer = static_cast<AudioTimer*>(voice->userdata);

	for (auto i = 0u; i < size; ++i) {
		if (const auto gain = voice->get_level(i, *timer)) {
			const auto bit_step = s32(voice->peek_phase(i) * 128.0f);
			const auto bit_mask = 1 << (0x7 ^ (bit_step & 0x7));
			::assign_cast_add(data[i], \
				(pulse_pattern_data()[bit_step >> 3] & bit_mask) ? gain : -gain);
		} else break;
	}
	voice->step_phase(size);
}

/*==================================================================*/

void XOCHIP::skip_instruction() noexcept {
	::assign_cast_add(m_current_pc, NNNN() == 0xF000 ? 4 : 2);
}

void XOCHIP::scroll_display_up(u32 N) noexcept {
	if (m_plane_mask & P0M) { m_display_map[0].shift(0, -s32(N)); }
	if (m_plane_mask & P1M) { m_display_map[1].shift(0, -s32(N)); }
	if (m_plane_mask & P2M) { m_display_map[2].shift(0, -s32(N)); }
	if (m_plane_mask & P3M) { m_display_map[3].shift(0, -s32(N)); }
}
void XOCHIP::scroll_display_dn(u32 N) noexcept {
	if (m_plane_mask & P0M) { m_display_map[0].shift(0, +s32(N)); }
	if (m_plane_mask & P1M) { m_display_map[1].shift(0, +s32(N)); }
	if (m_plane_mask & P2M) { m_display_map[2].shift(0, +s32(N)); }
	if (m_plane_mask & P3M) { m_display_map[3].shift(0, +s32(N)); }
}
void XOCHIP::scroll_display_lt() noexcept {
	if (m_plane_mask & P0M) { m_display_map[0].shift(-4, 0); }
	if (m_plane_mask & P1M) { m_display_map[1].shift(-4, 0); }
	if (m_plane_mask & P2M) { m_display_map[2].shift(-4, 0); }
	if (m_plane_mask & P3M) { m_display_map[3].shift(-4, 0); }
}
void XOCHIP::scroll_display_rt() noexcept {
	if (m_plane_mask & P0M) { m_display_map[0].shift(+4, 0); }
	if (m_plane_mask & P1M) { m_display_map[1].shift(+4, 0); }
	if (m_plane_mask & P2M) { m_display_map[2].shift(+4, 0); }
	if (m_plane_mask & P3M) { m_display_map[3].shift(+4, 0); }
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void XOCHIP::instruction_00CN(u32 N) noexcept {
		if (N) { scroll_display_dn(N); }
		trigger_interrupt(Interrupt::FRAME, Quirk.await_scroll);
	}
	void XOCHIP::instruction_00DN(u32 N) noexcept {
		if (N) { scroll_display_up(N); }
		trigger_interrupt(Interrupt::FRAME, Quirk.await_scroll);
	}
	void XOCHIP::instruction_00E0() noexcept {
		if (m_plane_mask & P0M) { m_display_map[P0].fill(); }
		if (m_plane_mask & P1M) { m_display_map[P1].fill(); }
		if (m_plane_mask & P2M) { m_display_map[P2].fill(); }
		if (m_plane_mask & P3M) { m_display_map[P3].fill(); }
	}
	void XOCHIP::instruction_00EE() noexcept {
		m_current_pc = m_stack_bank[--m_stack_head & 0xF];
	}
	void XOCHIP::instruction_00FB() noexcept {
		scroll_display_rt();
		trigger_interrupt(Interrupt::FRAME, Quirk.await_scroll);
	}
	void XOCHIP::instruction_00FC() noexcept {
		scroll_display_lt();
		trigger_interrupt(Interrupt::FRAME, Quirk.await_scroll);
	}
	void XOCHIP::instruction_00FD() noexcept {
		trigger_interrupt(Interrupt::SOUND);
	}
	void XOCHIP::instruction_00FE() noexcept {
		use_hires_screen(false);
		m_display_map[P0].resize(c_sys_screen_W/2, c_sys_screen_H/2).fill();
		m_display_map[P1].resize(c_sys_screen_W/2, c_sys_screen_H/2).fill();
		m_display_map[P2].resize(c_sys_screen_W/2, c_sys_screen_H/2).fill();
		m_display_map[P3].resize(c_sys_screen_W/2, c_sys_screen_H/2).fill();
	}
	void XOCHIP::instruction_00FF() noexcept {
		use_hires_screen(true);
		m_display_map[P0].resize(c_sys_screen_W, c_sys_screen_H).fill();
		m_display_map[P1].resize(c_sys_screen_W, c_sys_screen_H).fill();
		m_display_map[P2].resize(c_sys_screen_W, c_sys_screen_H).fill();
		m_display_map[P3].resize(c_sys_screen_W, c_sys_screen_H).fill();
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void XOCHIP::instruction_1NNN(u32 NNN) noexcept {
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void XOCHIP::instruction_2NNN(u32 NNN) noexcept {
		m_stack_bank[m_stack_head++ & 0xF] = m_current_pc;
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void XOCHIP::instruction_3xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] == NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void XOCHIP::instruction_4xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] != NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void XOCHIP::instruction_5xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] == m_registers_V[Y]) { skip_instruction(); }
	}
	void XOCHIP::instruction_5xy2(u32 X, u32 Y) noexcept {
		if (X < Y) {
			for (auto i = X; i <= Y; ++i) {
				m_memory_bank[m_register_I + i - X] = m_registers_V[i];
			}
		} else {
			for (auto i = X; i >= Y; --i) {
				m_memory_bank[m_register_I + X - i] = m_registers_V[i];
			}
		}
	}
	void XOCHIP::instruction_5xy3(u32 X, u32 Y) noexcept {
		if (X < Y) {
			for (auto i = X; i <= Y; ++i) {
				m_registers_V[i] = m_memory_bank[m_register_I + i - X];
			}
		} else {
			for (auto i = X; i >= Y; --i) {
				m_registers_V[i] = m_memory_bank[m_register_I + X - i];
			}
		}
	}
	void XOCHIP::instruction_5xy4(u32 X, u32 Y) noexcept {
		if (X < Y) {
			for (auto i = X; i <= Y; ++i) {
				m_bit_colors[i] = c_color_palette[m_memory_bank[m_register_I + i - X]];
			}
		} else {
			for (auto i = X; i >= Y; --i) {
				m_bit_colors[i] = c_color_palette[m_memory_bank[m_register_I + X - i]];
			}
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void XOCHIP::instruction_6xNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void XOCHIP::instruction_7xNN(u32 X, u32 NN) noexcept {
		::assign_cast_add(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void XOCHIP::instruction_8xy0(u32 X, u32 Y) noexcept {
		::assign_cast(m_registers_V[X], m_registers_V[Y]);
	}
	void XOCHIP::instruction_8xy1(u32 X, u32 Y) noexcept {
		::assign_cast_or(m_registers_V[X], m_registers_V[Y]);
	}
	void XOCHIP::instruction_8xy2(u32 X, u32 Y) noexcept {
		::assign_cast_and(m_registers_V[X], m_registers_V[Y]);
	}
	void XOCHIP::instruction_8xy3(u32 X, u32 Y) noexcept {
		::assign_cast_xor(m_registers_V[X], m_registers_V[Y]);
	}
	void XOCHIP::instruction_8xy4(u32 X, u32 Y) noexcept {
		const auto sum = m_registers_V[X] + m_registers_V[Y];
		::assign_cast(m_registers_V[X], sum);
		::assign_cast(m_registers_V[0xF], sum >> 8);
	}
	void XOCHIP::instruction_8xy5(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[X] >= m_registers_V[Y];
		::assign_cast_sub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void XOCHIP::instruction_8xy7(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[Y] >= m_registers_V[X];
		::assign_cast_rsub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void XOCHIP::instruction_8xy6(u32 X, u32 Y) noexcept {
		if (!Quirk.shift_vx_reg) { m_registers_V[X] = m_registers_V[Y]; }
		const bool lsb = (m_registers_V[X] & 1) == 1;
		::assign_cast_shr(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], lsb);
	}
	void XOCHIP::instruction_8xyE(u32 X, u32 Y) noexcept {
		if (!Quirk.shift_vx_reg) { m_registers_V[X] = m_registers_V[Y]; }
		const bool msb = (m_registers_V[X] >> 7) == 1;
		::assign_cast_shl(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void XOCHIP::instruction_9xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] != m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void XOCHIP::instruction_ANNN(u32 NNN) noexcept {
		::assign_cast(m_register_I, NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void XOCHIP::instruction_BNNN(u32 NNN) noexcept {
		jump_program_to(NNN + m_registers_V[0]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void XOCHIP::instruction_CxNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], m_rng->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void XOCHIP::draw_byte(u32 X, u32 Y, u32 P, u32 DATA) noexcept {
		switch (DATA) {
			[[unlikely]]
			case 0b00000000:
				return;

			[[unlikely]]
			case 0b10000000:
				if (Quirk.wrap_sprites) { X &= (m_display_map[P].width() - 1); }
				if (X < m_display_map[P].width()) {
					if (!((m_display_map[P](X, Y) ^= 1) & 1))
						{ m_registers_V[0xF] = 1; }
				}
				return;

			[[likely]]
			default:
				if (Quirk.wrap_sprites) { X &= (m_display_map[P].width() - 1); }
				else if (X >= m_display_map[P].width()) { return; }

				for (auto B = 0; B < 8; ++B, ++X &= (m_display_map[P].width() - 1)) {
					if (DATA & 0x80 >> B) {
						if (!((m_display_map[P](X, Y) ^= 1) & 1))
							{ m_registers_V[0xF] = 1; }
					}
					if (!Quirk.wrap_sprites && X == (m_display_map[P].width() - 1)) { return; }
				}
				return;
		}
	}

	template <std::size_t P>
	void XOCHIP::draw_single_row(u32 X, u32 Y) noexcept {
		const auto I = m_register_I + s_plane_mask[P][m_plane_mask];

		draw_byte(X, Y, P, m_memory_bank[I]);
	}

	template <std::size_t P>
	void XOCHIP::draw_double_row(u32 X, u32 Y) noexcept {
		const auto I = m_register_I + s_plane_mask[P][m_plane_mask] * 32;

		for (auto H = 0u; H < 16u; ++H) {
			draw_byte(X + 0, Y, P, m_memory_bank[I + H * 2 + 0]);
			draw_byte(X + 8, Y, P, m_memory_bank[I + H * 2 + 1]);

			if (!Quirk.wrap_sprites && Y == (m_display_map[P].height() - 1)) { break; }
			else { ++Y &= (m_display_map[P].height() - 1); }
		}
	}

	template <std::size_t P>
	void XOCHIP::draw_n_rows(u32 X, u32 Y, u32 N) noexcept {
		const auto I = m_register_I + s_plane_mask[P][m_plane_mask] * N;

		for (auto H = 0u; H < N; ++H) {
			draw_byte(X, Y, P, m_memory_bank[I + H]);

			if (!Quirk.wrap_sprites && Y == (m_display_map[P].height() - 1)) { break; }
			else { ++Y &= (m_display_map[P].height() - 1); }
		}
	}

	void XOCHIP::instruction_DxyN(u32 X, u32 Y, u32 N) noexcept {
		const auto pX = m_registers_V[X] & (m_display_map[0].width()  - 1);
		const auto pY = m_registers_V[Y] & (m_display_map[0].height() - 1);

		m_registers_V[0xF] = 0;

		switch (N) {
			case 0:
				if (m_plane_mask & P0M) { draw_double_row<P0>(pX, pY); }
				if (m_plane_mask & P1M) { draw_double_row<P1>(pX, pY); }
				if (m_plane_mask & P2M) { draw_double_row<P2>(pX, pY); }
				if (m_plane_mask & P3M) { draw_double_row<P3>(pX, pY); }
				break;

			case 1:
				if (m_plane_mask & P0M) { draw_single_row<P0>(pX, pY); }
				if (m_plane_mask & P1M) { draw_single_row<P1>(pX, pY); }
				if (m_plane_mask & P2M) { draw_single_row<P2>(pX, pY); }
				if (m_plane_mask & P3M) { draw_single_row<P3>(pX, pY); }
				break;

			default:
				if (m_plane_mask & P0M) { draw_n_rows<P0>(pX, pY, N); }
				if (m_plane_mask & P1M) { draw_n_rows<P1>(pX, pY, N); }
				if (m_plane_mask & P2M) { draw_n_rows<P2>(pX, pY, N); }
				if (m_plane_mask & P3M) { draw_n_rows<P3>(pX, pY, N); }
				break;
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void XOCHIP::instruction_Ex9E(u32 X) noexcept {
		if (is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}
	void XOCHIP::instruction_ExA1(u32 X) noexcept {
		if (!is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void XOCHIP::instruction_F000() noexcept {
		::assign_cast(m_register_I, NNNN());
		::assign_cast_add(m_current_pc, 2);
	}
	void XOCHIP::instruction_F002() noexcept {
		for (auto i = 0u; i < 16u; ++i) {
			pulse_pattern_data()[i] = m_memory_bank[m_register_I + i];
		}
	}
	void XOCHIP::instruction_FN01(u32 N) noexcept {
		m_plane_mask = N;
	}
	void XOCHIP::instruction_Fx07(u32 X) noexcept {
		::assign_cast(m_registers_V[X], m_delay_timer);
	}
	void XOCHIP::instruction_Fx0A(u32 X) noexcept {
		m_key_reg_ref = &m_registers_V[X];
		trigger_interrupt(Interrupt::INPUT);
	}
	void XOCHIP::instruction_Fx15(u32 X) noexcept {
		::assign_cast(m_delay_timer, m_registers_V[X]);
	}
	void XOCHIP::instruction_Fx18(u32 X) noexcept {
		m_audio_timers[VOICE::UNIQUE].set(m_registers_V[X] + (m_registers_V[X] == 1));
	}
	void XOCHIP::instruction_Fx1E(u32 X) noexcept {
		::assign_cast_add(m_register_I, m_registers_V[X]);
	}
	void XOCHIP::instruction_Fx29(u32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 5 + c_small_font_offset);
	}
	void XOCHIP::instruction_Fx30(u32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 10 + c_large_font_offset);
	}
	void XOCHIP::instruction_Fx33(u32 X) noexcept {
		const TriBCD bcd{ m_registers_V[X] };

		m_memory_bank[m_register_I + 0] = bcd.digit[2];
		m_memory_bank[m_register_I + 1] = bcd.digit[1];
		m_memory_bank[m_register_I + 2] = bcd.digit[0];
	}
	void XOCHIP::instruction_Fx3A(u32 X) noexcept {
		set_pattern_pitch(m_registers_V[X]);
	}
	void XOCHIP::instruction_FN55(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_memory_bank[m_register_I + i] = m_registers_V[i]; }
		if (!Quirk.no_inc_i_reg) [[likely]] { ::assign_cast_add(m_register_I, N + 1); }
	}
	void XOCHIP::instruction_FN65(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_registers_V[i] = m_memory_bank[m_register_I + i]; }
		if (!Quirk.no_inc_i_reg) [[likely]] { ::assign_cast_add(m_register_I, N + 1); }
	}
	void XOCHIP::instruction_FN75(u32 N) noexcept {
		set_permaregs(N + 1);
	}
	void XOCHIP::instruction_FN85(u32 N) noexcept {
		get_permaregs(N + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/


#endif
