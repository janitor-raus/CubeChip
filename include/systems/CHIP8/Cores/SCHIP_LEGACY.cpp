/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "SCHIP_LEGACY.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_SCHIP_LEGACY)

#include "CoreRegistry.hpp"

REGISTER_CORE(SCHIP_LEGACY, ".sc8")

/*==================================================================*/

void SCHIP_LEGACY::initialize_system() noexcept {
	::generate_n(m_memory_bank, 0, c_sys_memory_size,
		[&]() noexcept { return m_rng->next<u8>(); });

	copy_game_to_memory(m_memory_bank.data() + c_game_load_pos);
	copy_font_to_memory(m_memory_bank.data(), 180);

	set_base_system_framerate(c_sys_refresh_rate);

	m_voices[VOICE::ID_0].userdata = &m_audio_timers[VOICE::ID_0];
	m_voices[VOICE::ID_1].userdata = &m_audio_timers[VOICE::ID_1];
	m_voices[VOICE::ID_2].userdata = &m_audio_timers[VOICE::ID_2];
	m_voices[VOICE::ID_3].userdata = &m_audio_timers[VOICE::ID_3];

	m_current_pc = c_sys_boot_pos;
	m_target_cpf = c_sys_speed_hi;

	Quirk.await_vblank = true;

	m_display_device.metadata_staging()
		.set_minimum_zoom(4).set_inner_margin(4)
		.set_texture_tint(s_bit_colors[0])
		.enabled = true;
}

void SCHIP_LEGACY::handle_cycle_loop() noexcept
	{ LOOP_DISPATCH(instruction_loop); }

template <typename Lambda>
void SCHIP_LEGACY::instruction_loop(Lambda&& condition) noexcept {
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
					CASE_xNF0(0xC0):
						instruction_00CN(_N);
						break;
					case 0x00E0:
						instruction_00E0();
						break;
					case 0x00EE:
						instruction_00EE();
						break;
					case 0x00FB:
						instruction_00FB();
						break;
					case 0x00FC:
						instruction_00FC();
						break;
					case 0x00FD:
						instruction_00FD();
						break;
					case 0x00FE:
						instruction_00FE();
						break;
					case 0x00FF:
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
				if (_N) [[unlikely]] {
					instruction_error(HI, LO);
				} else {
					instruction_5xy0(_X, Y_);
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
				instruction_BXNN(_X, _NNN);
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
					case 0x30:
						instruction_Fx30(_X);
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

void SCHIP_LEGACY::push_audio_data() {
	mix_audio_data({
		{ make_pulse_wave, &m_voices[VOICE::ID_0] },
		{ make_pulse_wave, &m_voices[VOICE::ID_1] },
		{ make_pulse_wave, &m_voices[VOICE::ID_2] },
		{ make_pulse_wave, &m_voices[VOICE::BUZZER] },
	});

	m_display_device.metadata_staging().set_border_color_if(
		!!::accumulate(m_audio_timers), s_bit_colors[1]);
}

void SCHIP_LEGACY::push_video_data() {
	m_display_device.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = ++m_display_device.metadata_staging();
		frame.copy_from(m_display_map, use_pixel_trails()
			? [](u32 pixel) noexcept { return RGBA::premul(s_bit_colors[pixel != 0], c_bit_weight[pixel]); }
			: [](u32 pixel) noexcept { return s_bit_colors[pixel >> 3]; }
		);
	});

	if (use_pixel_trails()) {
		std::for_each(EXEC_POLICY(unseq)
			m_display_map.begin(), m_display_map.end(),
			[](auto& pixel) noexcept {
				::assign_cast(pixel, (pixel & 0x8) | (pixel >> 1));
			}
		);
	}
}

/*==================================================================*/

void SCHIP_LEGACY::scroll_display_dn(u32 N) {
	m_display_map.shift(0, +N);
}
void SCHIP_LEGACY::scroll_display_lt() {
	m_display_map.shift(-4, 0);
}
void SCHIP_LEGACY::scroll_display_rt() {
	m_display_map.shift(+4, 0);
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void SCHIP_LEGACY::instruction_00CN(u32 N) noexcept {
		scroll_display_dn(N);
	}
	void SCHIP_LEGACY::instruction_00E0() noexcept {
		m_display_map.fill();
		trigger_interrupt(Interrupt::FRAME);
	}
	void SCHIP_LEGACY::instruction_00EE() noexcept {
		m_current_pc = m_stack_bank[--m_stack_head & 0xF];
	}
	void SCHIP_LEGACY::instruction_00FB() noexcept {
		scroll_display_rt();
	}
	void SCHIP_LEGACY::instruction_00FC() noexcept {
		scroll_display_lt();
	}
	void SCHIP_LEGACY::instruction_00FD() noexcept {
		trigger_interrupt(Interrupt::SOUND);
	}
	void SCHIP_LEGACY::instruction_00FE() noexcept {
		use_hires_screen(false);
		Quirk.await_vblank = true;
		m_target_cpf = c_sys_speed_hi;
		trigger_interrupt(Interrupt::FRAME);
	}
	void SCHIP_LEGACY::instruction_00FF() noexcept {
		use_hires_screen(true);
		Quirk.await_vblank = false;
		m_target_cpf = c_sys_speed_lo;
		trigger_interrupt(Interrupt::FRAME);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void SCHIP_LEGACY::instruction_1NNN(u32 NNN) noexcept {
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void SCHIP_LEGACY::instruction_2NNN(u32 NNN) noexcept {
		::assign_cast(m_stack_bank[m_stack_head++ & 0xF], m_current_pc);
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void SCHIP_LEGACY::instruction_3xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] == NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void SCHIP_LEGACY::instruction_4xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] != NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void SCHIP_LEGACY::instruction_5xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] == m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void SCHIP_LEGACY::instruction_6xNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void SCHIP_LEGACY::instruction_7xNN(u32 X, u32 NN) noexcept {
		::assign_cast_add(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void SCHIP_LEGACY::instruction_8xy0(u32 X, u32 Y) noexcept {
		::assign_cast(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy1(u32 X, u32 Y) noexcept {
		::assign_cast_or(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy2(u32 X, u32 Y) noexcept {
		::assign_cast_and(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy3(u32 X, u32 Y) noexcept {
		::assign_cast_xor(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_LEGACY::instruction_8xy4(u32 X, u32 Y) noexcept {
		const auto sum = m_registers_V[X] + m_registers_V[Y];
		::assign_cast(m_registers_V[X], sum);
		::assign_cast(m_registers_V[0xF], sum >> 8);
	}
	void SCHIP_LEGACY::instruction_8xy5(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[X] >= m_registers_V[Y];
		::assign_cast_sub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void SCHIP_LEGACY::instruction_8xy7(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[Y] >= m_registers_V[X];
		::assign_cast_rsub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void SCHIP_LEGACY::instruction_8xy6(u32 X, u32  ) noexcept {
		const bool lsb = (m_registers_V[X] & 1) == 1;
		::assign_cast_shr(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], lsb);
	}
	void SCHIP_LEGACY::instruction_8xyE(u32 X, u32  ) noexcept {
		const bool msb = (m_registers_V[X] >> 7) == 1;
		::assign_cast_shl(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void SCHIP_LEGACY::instruction_9xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] != m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void SCHIP_LEGACY::instruction_ANNN(u32 NNN) noexcept {
		::assign_cast(m_register_I, NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void SCHIP_LEGACY::instruction_BXNN(u32 X, u32 NNN) noexcept {
		jump_program_to(NNN + m_registers_V[X]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void SCHIP_LEGACY::instruction_CxNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], m_rng->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	bool SCHIP_LEGACY::draw_single_byte(
		u32 originX, u32 originY,
		u32 WIDTH,   u32 DATA
	) noexcept {
		if (!DATA) { return false; }
		bool collided = false;

		for (auto B = 0u; B < WIDTH; ++B) {
			const auto offsetX = originX + B;

			if (DATA >> (WIDTH - 1 - B) & 0x1) {
				auto& pixel = m_display_map(offsetX, originY);
				if (!((pixel ^= 0x8) & 0x8)) { collided = true; }
			}
			if (offsetX == (c_sys_screen_W - 1)) { return collided; }
		}
		return collided;
	}

	bool SCHIP_LEGACY::draw_double_byte(
		u32 originX, u32 originY,
		u32 WIDTH,   u32 DATA
	) noexcept {
		if (!DATA) { return false; }
		bool collided = false;

		for (auto B = 0u; B < WIDTH; ++B) {
			const auto offsetX = originX + B;

			auto& pixelHI = m_display_map(offsetX, originY + 0);
			auto& pixelLO = m_display_map(offsetX, originY + 1);

			if (DATA >> (WIDTH - 1 - B) & 0x1) {
				if (pixelHI & 0x8) { collided = true; }
				pixelLO = pixelHI ^= 0x8;
			} else {
				pixelLO = pixelHI;
			}
			if (offsetX == (c_sys_screen_W - 1)) { return collided; }
		}
		return collided;
	}

	void SCHIP_LEGACY::instruction_DxyN(u32 X, u32 Y, u32 N) noexcept {
		if (use_hires_screen()) {
			const auto offsetX = 8 - (m_registers_V[X] & 7);
			const auto originX = m_registers_V[X] & 0x78;
			const auto originY = m_registers_V[Y] & 0x3F;

			auto collisions = 0;

			if (N == 0) {
				for (auto rowN = 0u; rowN < 16u; ++rowN) {
					const auto offsetY = originY + rowN;

					collisions += draw_single_byte(originX, offsetY, offsetX ? 24 : 16, (
						m_memory_bank[m_register_I + 2 * rowN + 0] << 8 |
						m_memory_bank[m_register_I + 2 * rowN + 1] << 0
					) << offsetX);

					if (offsetY == (c_sys_screen_H - 1)) { break; }
				}
			} else {
				for (auto rowN = 0u; rowN < N; ++rowN) {
					const auto offsetY = originY + rowN;

					collisions += draw_single_byte(originX, offsetY, offsetX ? 16 : 8,
						m_memory_bank[m_register_I + rowN] << offsetX);

					if (offsetY == (c_sys_screen_H - 1)) { break; }
				}
			}
			::assign_cast(m_registers_V[0xF], collisions);
		}
		else {
			const auto offsetX = 16 - 2 * (m_registers_V[X] & 0x07);
			const auto originX = m_registers_V[X] * 2 & 0x70;
			const auto originY = m_registers_V[Y] * 2 & 0x3F;
			const auto lengthN = N == 0u ? 16u : N;

			auto collisions = 0;

			for (auto rowN = 0u; rowN < lengthN; ++rowN) {
				const auto offsetY = originY + rowN * 2;

				collisions += draw_double_byte(originX, offsetY, 0x20,
					ez::bit_dup8(m_memory_bank[m_register_I + rowN]) << offsetX);

				if (offsetY == (c_sys_screen_H - 2)) { break; }
			}
			::assign_cast(m_registers_V[0xF], collisions != 0);
		}

		trigger_interrupt(Interrupt::FRAME, Quirk.await_vblank);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void SCHIP_LEGACY::instruction_Ex9E(u32 X) noexcept {
		if (is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}
	void SCHIP_LEGACY::instruction_ExA1(u32 X) noexcept {
		if (!is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void SCHIP_LEGACY::instruction_Fx07(u32 X) noexcept {
		::assign_cast(m_registers_V[X], m_delay_timer);
	}
	void SCHIP_LEGACY::instruction_Fx0A(u32 X) noexcept {
		m_key_reg_ref = &m_registers_V[X];
		trigger_interrupt(Interrupt::INPUT);
	}
	void SCHIP_LEGACY::instruction_Fx15(u32 X) noexcept {
		::assign_cast(m_delay_timer, m_registers_V[X]);
	}
	void SCHIP_LEGACY::instruction_Fx18(u32 X) noexcept {
		start_voice(m_registers_V[X] + (m_registers_V[X] == 1));
	}
	void SCHIP_LEGACY::instruction_Fx1E(u32 X) noexcept {
		::assign_cast_add(m_register_I, m_registers_V[X]);
	}
	void SCHIP_LEGACY::instruction_Fx29(u32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 5 + c_small_font_offset);
	}
	void SCHIP_LEGACY::instruction_Fx30(u32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 10 + c_large_font_offset);
	}
	void SCHIP_LEGACY::instruction_Fx33(u32 X) noexcept {
		const TriBCD bcd{ m_registers_V[X] };

		m_memory_bank[m_register_I + 0] = bcd.digit[2];
		m_memory_bank[m_register_I + 1] = bcd.digit[1];
		m_memory_bank[m_register_I + 2] = bcd.digit[0];
	}
	void SCHIP_LEGACY::instruction_FN55(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_memory_bank[m_register_I + i] = m_registers_V[i]; }
		if (Quirk.x1_inc_i_reg) [[likely]] { ::assign_cast_add(m_register_I, N); }
	}
	void SCHIP_LEGACY::instruction_FN65(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_registers_V[i] = m_memory_bank[m_register_I + i]; }
		if (Quirk.x1_inc_i_reg) [[likely]] { ::assign_cast_add(m_register_I, N); }
	}
	void SCHIP_LEGACY::instruction_FN75(u32 N) noexcept {
		set_permaregs(std::min(N, 7u) + 1);
	}
	void SCHIP_LEGACY::instruction_FN85(u32 N) noexcept {
		get_permaregs(std::min(N, 7u) + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
