/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "CHIP8X.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_CHIP8X)

#include "CoreRegistry.inl"

REGISTER_SYSTEM_CORE(CHIP8X)

/*==================================================================*/

void CHIP8X::initialize_system() noexcept {
	::generate_n(m_memory_bank, 0, c_sys_memory_size,
		[&]() noexcept { return u8(m_rng->next()); });

	copy_file_image_to(m_memory_bank, c_game_load_pos);
	copy_font_data_to(m_memory_bank, 80);

	set_base_system_framerate(c_sys_refresh_rate);

	m_voices[VOICE::UNIQUE].userdata = &m_audio_timers[VOICE::UNIQUE];
	m_voices[VOICE::BUZZER].userdata = &m_audio_timers[VOICE::BUZZER];

	m_current_pc = c_sys_boot_pos;
	m_target_cpf = c_sys_speed_hi;

	// test first color rect as the original hardware did
	m_colored_map(0, 0) = c_fore_colors[2];

	auto& meta = m_display_device.metadata_staging();

	meta.minimum_zoom = 8;
	meta.inner_margin = 4;
	meta.texture_tint = c_back_colors[m_background_color];
	meta.enabled = true;
}

void CHIP8X::handle_cycle_loop() noexcept
	{ LOOP_DISPATCH(instruction_loop); }

template <typename Lambda>
void CHIP8X::instruction_loop(Lambda&& condition) noexcept {
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
					case 0xE0:
						instruction_00E0();
						break;
					case 0xEE:
						instruction_00EE();
						break;
					[[unlikely]]
					default: instruction_error(HI, LO);
				}
				break;
			case 0x02:
				if (LO){
					instruction_02A0();
				} else [[unlikely]] {
					instruction_error(HI, LO);
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
					CASE_xFN(0x01):
						instruction_5xy1(_X, Y_);
						break;
					[[unlikely]]
					default: instruction_error(HI, LO);
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
				if (HI == 0xBF) [[unlikely]] {
					instruction_error(HI, LO);
				} else {
					instruction_BxyN(_X, Y_, _N);
				}
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
					case 0xF2:
						instruction_ExF2(_X);
						break;
					case 0xF5:
						instruction_ExF5(_X);
						break;
					[[unlikely]]
					default: instruction_error(HI, LO);
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
					case 0x33:
						instruction_Fx33(_X);
						break;
					case 0x55:
						instruction_FN55(_X);
						break;
					case 0x65:
						instruction_FN65(_X);
						break;
					case 0xF8:
						instruction_FxF8(_X);
						break;
					case 0xFB:
						instruction_FxFB(_X);
						break;
					[[unlikely]]
					default: instruction_error(HI, LO);
				}
				break;
		}
	}
}

void CHIP8X::push_audio_data() noexcept {
	mix_audio_data({
		{ make_pulse_wave, &m_voices[VOICE::UNIQUE] },
		{ make_pulse_wave, &m_voices[VOICE::BUZZER] },
	});

	static constexpr u32 idx[]{ 2, 7, 4, 1 };
	m_display_device.metadata_staging().set_border_color_if(
		!!::accumulate(m_audio_timers, 0), c_fore_colors[idx[m_background_color]]);
}

void CHIP8X::push_video_data() noexcept {
	if (use_pixel_trails()) {
		m_display_device.swapchain().acquire([&](auto& frame) noexcept {
			frame.metadata = ++m_display_device.metadata_staging();
			frame.copy_from(m_display_map, [&](auto& pixel) noexcept {
				if (pixel == 0) {
					return c_back_colors[m_background_color];
				} else {
					const auto idx = &pixel - m_display_map.data();
					const auto y = (idx / c_sys_screen_W) & m_color_pixel_mask;
					const auto x = (idx % c_sys_screen_W) >> 3;
					return RGBA::premul(m_colored_map(x, y), c_bit_weight[pixel]);
				}
			});
		});

		std::for_each(EXEC_POLICY(unseq)
			m_display_map.begin(), m_display_map.end(),
			[](auto& pixel) noexcept {
				::assign_cast(pixel, (pixel & 0x8) | (pixel >> 1));
			}
		);
	} else {
		m_display_device.swapchain().acquire([&](auto& frame) noexcept {
			frame.metadata = ++m_display_device.metadata_staging();
			frame.copy_from(m_display_map, [&](auto& pixel) noexcept {
				if (pixel == 0) {
					return c_back_colors[m_background_color];
				} else {
					const auto idx = &pixel - m_display_map.data();
					const auto y = (idx / c_sys_screen_W) & m_color_pixel_mask;
					const auto x = (idx % c_sys_screen_W) >> 3;
					return m_colored_map(x, y);
				}
			});
		});
	}
}

void CHIP8X::set_pulse_pitch(u32 pitch) noexcept {
	if (auto* stream = m_audio_device.at(STREAM::MAIN)) {
		m_voices[VOICE::UNIQUE].set_step((c_tonal_offset + (
			(0xFF - (pitch ? pitch : 0x80)) >> 3 << 4)
		) / stream->get_freq() * get_framerate_multiplier());
	}
}

void CHIP8X::color_lores_zone(u32 X, u32 Y, u32 idx) noexcept {
	for (auto pY = 0u, maxH = Y >> 4; pY <= maxH; ++pY) {
		for (auto pX = 0u, maxW = X >> 4; pX <= maxW; ++pX) {
			m_colored_map((X + pX) & 0x7, ((Y + pY) << 2) & 0x1F) = c_fore_colors[idx & 0x7];
		}
	}
	m_color_pixel_mask = 0xFC;
}

void CHIP8X::color_hires_zone(u32 X, u32 Y, u32 idx, u32 N) noexcept {
	for (auto pY = Y, pX = X >> 3; pY < Y + N; ++pY) {
		m_colored_map(pX & 0x7, pY & 0x1F) = c_fore_colors[idx & 0x7];
	}
	m_color_pixel_mask = 0xFF;
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void CHIP8X::instruction_00E0() noexcept {
		m_display_map.fill();
		trigger_interrupt(Interrupt::FRAME);
	}
	void CHIP8X::instruction_00EE() noexcept {
		m_current_pc = m_stack_bank[--m_stack_head & 0xF];
	}
	void CHIP8X::instruction_02A0() noexcept {
		m_display_device.metadata_staging().texture_tint
			= c_back_colors[++m_background_color &= 0x3];
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void CHIP8X::instruction_1NNN(u32 NNN) noexcept {
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void CHIP8X::instruction_2NNN(u32 NNN) noexcept {
		m_stack_bank[m_stack_head++ & 0xF] = m_current_pc;
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void CHIP8X::instruction_3xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] == NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void CHIP8X::instruction_4xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] != NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void CHIP8X::instruction_5xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] == m_registers_V[Y]) { skip_instruction(); }
	}
	void CHIP8X::instruction_5xy1(u32 X, u32 Y) noexcept {
		const auto lenX = (m_registers_V[X] & 0x70) + (m_registers_V[Y] & 0x70);
		const auto lenY = (m_registers_V[X] + m_registers_V[Y]) & 0x7;
		::assign_cast(m_registers_V[X], lenX | lenY);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void CHIP8X::instruction_6xNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void CHIP8X::instruction_7xNN(u32 X, u32 NN) noexcept {
		::assign_cast_add(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void CHIP8X::instruction_8xy0(u32 X, u32 Y) noexcept {
		::assign_cast(m_registers_V[X], m_registers_V[Y]);
	}
	void CHIP8X::instruction_8xy1(u32 X, u32 Y) noexcept {
		::assign_cast_or(m_registers_V[X], m_registers_V[Y]);
	}
	void CHIP8X::instruction_8xy2(u32 X, u32 Y) noexcept {
		::assign_cast_and(m_registers_V[X], m_registers_V[Y]);
	}
	void CHIP8X::instruction_8xy3(u32 X, u32 Y) noexcept {
		::assign_cast_xor(m_registers_V[X], m_registers_V[Y]);
	}
	void CHIP8X::instruction_8xy4(u32 X, u32 Y) noexcept {
		const auto sum = m_registers_V[X] + m_registers_V[Y];
		::assign_cast(m_registers_V[X], sum);
		::assign_cast(m_registers_V[0xF], sum >> 8);
	}
	void CHIP8X::instruction_8xy5(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[X] >= m_registers_V[Y];
		::assign_cast_sub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void CHIP8X::instruction_8xy7(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[Y] >= m_registers_V[X];
		::assign_cast_rsub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void CHIP8X::instruction_8xy6(u32 X, u32 Y) noexcept {
		const bool lsb = (m_registers_V[Y] & 1) == 1;
		::assign_cast(m_registers_V[X], m_registers_V[Y] >> 1);
		::assign_cast(m_registers_V[0xF], lsb);
	}
	void CHIP8X::instruction_8xyE(u32 X, u32 Y) noexcept {
		const bool msb = (m_registers_V[Y] >> 7) == 1;
		::assign_cast(m_registers_V[X], m_registers_V[Y] << 1);
		::assign_cast(m_registers_V[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void CHIP8X::instruction_9xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] != m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void CHIP8X::instruction_ANNN(u32 NNN) noexcept {
		::assign_cast(m_register_I, NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void CHIP8X::instruction_BxyN(u32 X, u32 Y, u32 N) noexcept {
		if (N) {
			color_hires_zone(m_registers_V[X], m_registers_V[X + 1], m_registers_V[Y] & 0x7, N);
		} else {
			color_lores_zone(m_registers_V[X], m_registers_V[X + 1], m_registers_V[Y] & 0x7);
		}
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void CHIP8X::instruction_CxNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], m_rng->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void CHIP8X::draw_byte(u32 X, u32 Y, u32 DATA) noexcept {
		switch (DATA) {
			[[unlikely]]
			case 0b00000000:
				return;

			[[likely]]
			case 0b10000000:
				if (!((m_display_map(X, Y) ^= 0x8) & 0x8))
					{ m_registers_V[0xF] = 1; }
				return;

			[[unlikely]]
			default:
				for (auto B = 0; B < 8; ++B) {
					if (DATA & 0x80 >> B) {
						if (!((m_display_map(X, Y) ^= 0x8) & 0x8))
							{ m_registers_V[0xF] = 1; }
					}
					if (++X == c_sys_screen_W) { return; }
				}
				return;
		}
	}

	void CHIP8X::instruction_DxyN(u32 X, u32 Y, u32 N) noexcept {
		auto pX = m_registers_V[X] & (c_sys_screen_W - 1);
		auto pY = m_registers_V[Y] & (c_sys_screen_H - 1);

		m_registers_V[0xF] = 0;

		switch (N) {
			[[unlikely]]
			case 0: break;

			[[likely]]
			case 1:
				draw_byte(pX, pY, m_memory_bank[m_register_I]);
				break;

			[[unlikely]]
			default:
				for (auto H = 0u; H < N; ++H)
				{
					draw_byte(pX, pY, m_memory_bank[m_register_I + H]);
					if (++pY == c_sys_screen_H) { break; }
				}
				break;
		}

		trigger_interrupt(Interrupt::FRAME);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void CHIP8X::instruction_Ex9E(u32 X) noexcept {
		if (is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}
	void CHIP8X::instruction_ExA1(u32 X) noexcept {
		if (!is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}
	void CHIP8X::instruction_ExF2(u32 X) noexcept {
		if (is_key_held_P2(m_registers_V[X])) { skip_instruction(); }
	}
	void CHIP8X::instruction_ExF5(u32 X) noexcept {
		if (!is_key_held_P2(m_registers_V[X])) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void CHIP8X::instruction_Fx07(u32 X) noexcept {
		::assign_cast(m_registers_V[X], m_delay_timer);
	}
	void CHIP8X::instruction_Fx0A(u32 X) noexcept {
		m_key_reg_ref = &m_registers_V[X];
		trigger_interrupt(Interrupt::INPUT);
	}
	void CHIP8X::instruction_Fx15(u32 X) noexcept {
		::assign_cast(m_delay_timer, m_registers_V[X]);
	}
	void CHIP8X::instruction_Fx18(u32 X) noexcept {
		m_audio_timers[VOICE::UNIQUE].set(m_registers_V[X] + (m_registers_V[X] == 1));
	}
	void CHIP8X::instruction_Fx1E(u32 X) noexcept {
		::assign_cast_add(m_register_I, m_registers_V[X]);
	}
	void CHIP8X::instruction_Fx29(u32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 5 + c_small_font_offset);
	}
	void CHIP8X::instruction_Fx33(u32 X) noexcept {
		const TriBCD bcd{ m_registers_V[X] };

		m_memory_bank[m_register_I + 0] = bcd.digit[2];
		m_memory_bank[m_register_I + 1] = bcd.digit[1];
		m_memory_bank[m_register_I + 2] = bcd.digit[0];
	}
	void CHIP8X::instruction_FN55(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_memory_bank[m_register_I + i] = m_registers_V[i]; }
		::assign_cast_add(m_register_I, N + 1);
	}
	void CHIP8X::instruction_FN65(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_registers_V[i] = m_memory_bank[m_register_I + i]; }
		::assign_cast_add(m_register_I, N + 1);
	}
	void CHIP8X::instruction_FxF8(u32 X) noexcept {
		set_pulse_pitch(m_registers_V[X]);
	}
	void CHIP8X::instruction_FxFB(u32) noexcept {
		trigger_interrupt(Interrupt::FRAME);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
