/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "SCHIP_MODERN.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_SCHIP_MODERN)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(SCHIP_MODERN, ".sc8")

/*==================================================================*/

void SCHIP_MODERN::initialize_system() noexcept {
	copy_game_to_memory(m_memory_bank.data() + c_game_load_pos);
	copy_font_to_memory(m_memory_bank.data(), 240);

	mDisplay.set(cScreenSizeX, cScreenSizeY);
	set_base_system_framerate(c_sys_refresh_rate);

	m_voices[VOICE::ID_0].userdata = &m_audio_timers[VOICE::ID_0];
	m_voices[VOICE::ID_1].userdata = &m_audio_timers[VOICE::ID_1];
	m_voices[VOICE::ID_2].userdata = &m_audio_timers[VOICE::ID_2];
	m_voices[VOICE::ID_3].userdata = &m_audio_timers[VOICE::ID_3];

	m_current_pc = c_sys_boot_pos;
	m_target_cpf = c_sys_speed_lo;

	set_display_properties(Resolution::LO);

	m_display_device.metadata_staging()
		.set_viewport(64, 32)
		.set_inner_margin(4)
		.set_texture_tint(s_bit_colors[0])
		.enabled = true;
}

void SCHIP_MODERN::handle_cycle_loop() noexcept
	{ LOOP_DISPATCH(instruction_loop); }

template <typename Lambda>
void SCHIP_MODERN::instruction_loop(Lambda&& condition) noexcept {
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

void SCHIP_MODERN::push_audio_data() {
	mix_audio_data({
		{ make_pulse_wave, &m_voices[VOICE::ID_0] },
		{ make_pulse_wave, &m_voices[VOICE::ID_1] },
		{ make_pulse_wave, &m_voices[VOICE::ID_2] },
		{ make_pulse_wave, &m_voices[VOICE::BUZZER] },
	});

	m_display_device.metadata_staging().set_border_color_if(
		!!::accumulate(m_audio_timers), s_bit_colors[1]);
}

void SCHIP_MODERN::push_video_data() {
	m_display_device.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = ++m_display_device.metadata_staging();
		frame.copy_from(m_display_buffer, use_pixel_trails()
			? [](u32 pixel) noexcept { return RGBA::premul(s_bit_colors[pixel != 0], c_bit_weight[pixel]); }
			: [](u32 pixel) noexcept { return s_bit_colors[pixel >> 3]; }
		);
	});

	std::for_each(EXEC_POLICY(unseq)
		m_display_buffer.begin(),
		m_display_buffer.end(),
		[](auto& pixel) noexcept
			{ ::assign_cast(pixel, (pixel & 0x8) | (pixel >> 1)); }
	);
}

void SCHIP_MODERN::set_display_properties(const Resolution mode) {
	use_hires_screen(mode != Resolution::LO);

	const auto W = use_hires_screen() ? c_sys_screen_W : c_sys_screen_W / 2;
	const auto H = use_hires_screen() ? c_sys_screen_H : c_sys_screen_H / 2;

	m_display_device.metadata_staging()
		.set_viewport(W, H)
		.set_minimum_zoom(use_hires_screen() ? 4 : 8);

	mDisplay.set(W, H);

	m_display_buffer.resizeClean(W, H);
};

/*==================================================================*/

void SCHIP_MODERN::scroll_display_dn(s32 N) {
	m_display_buffer.shift(0, +N);
}
void SCHIP_MODERN::scroll_display_lt() {
	m_display_buffer.shift(-4, 0);
}
void SCHIP_MODERN::scroll_display_rt() {
	m_display_buffer.shift(+4, 0);
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void SCHIP_MODERN::instruction_00CN(s32 N) noexcept {
		if (N) { scroll_display_dn(N); }
		if (Quirk.await_scroll) [[unlikely]]
			{ trigger_interrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00E0() noexcept {
		m_display_buffer.initialize();
		if (Quirk.await_vblank) [[unlikely]]
			{ trigger_interrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00EE() noexcept {
		m_current_pc = m_stack_bank[--m_stack_head & 0xF];
	}
	void SCHIP_MODERN::instruction_00FB() noexcept {
		scroll_display_rt();
		if (Quirk.await_scroll) [[unlikely]]
			{ trigger_interrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00FC() noexcept {
		scroll_display_lt();
		if (Quirk.await_scroll) [[unlikely]]
			{ trigger_interrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00FD() noexcept {
		trigger_interrupt(Interrupt::SOUND);
	}
	void SCHIP_MODERN::instruction_00FE() noexcept {
		set_display_properties(Resolution::LO);
		if (Quirk.await_vblank) [[unlikely]]
			{ trigger_interrupt(Interrupt::FRAME); }
	}
	void SCHIP_MODERN::instruction_00FF() noexcept {
		set_display_properties(Resolution::HI);
		if (Quirk.await_vblank) [[unlikely]]
			{ trigger_interrupt(Interrupt::FRAME); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void SCHIP_MODERN::instruction_1NNN(s32 NNN) noexcept {
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void SCHIP_MODERN::instruction_2NNN(s32 NNN) noexcept {
		m_stack_bank[m_stack_head++ & 0xF] = m_current_pc;
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void SCHIP_MODERN::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (m_registers_V[X] == NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void SCHIP_MODERN::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (m_registers_V[X] != NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void SCHIP_MODERN::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (m_registers_V[X] == m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void SCHIP_MODERN::instruction_6xNN(s32 X, s32 NN) noexcept {
		::assign_cast(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void SCHIP_MODERN::instruction_7xNN(s32 X, s32 NN) noexcept {
		::assign_cast_add(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void SCHIP_MODERN::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_MODERN::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_MODERN::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_MODERN::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(m_registers_V[X], m_registers_V[Y]);
	}
	void SCHIP_MODERN::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum = m_registers_V[X] + m_registers_V[Y];
		::assign_cast(m_registers_V[X], sum);
		::assign_cast(m_registers_V[0xF], sum >> 8);
	}
	void SCHIP_MODERN::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow = m_registers_V[X] >= m_registers_V[Y];
		::assign_cast_sub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void SCHIP_MODERN::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow = m_registers_V[Y] >= m_registers_V[X];
		::assign_cast_rsub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void SCHIP_MODERN::instruction_8xy6(s32 X, s32 Y) noexcept {
		if (!Quirk.shift_vx_reg) { m_registers_V[X] = m_registers_V[Y]; }
		const bool lsb = (m_registers_V[X] & 1) == 1;
		::assign_cast_shr(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], lsb);
	}
	void SCHIP_MODERN::instruction_8xyE(s32 X, s32 Y) noexcept {
		if (!Quirk.shift_vx_reg) { m_registers_V[X] = m_registers_V[Y]; }
		const bool msb = (m_registers_V[X] >> 7) == 1;
		::assign_cast_shl(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void SCHIP_MODERN::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (m_registers_V[X] != m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void SCHIP_MODERN::instruction_ANNN(s32 NNN) noexcept {
		::assign_cast(m_register_I, NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void SCHIP_MODERN::instruction_BNNN(s32 NNN) noexcept {
		jump_program_to(NNN + m_registers_V[0]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void SCHIP_MODERN::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(m_registers_V[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void SCHIP_MODERN::drawByte(s32 X, s32 Y, u32 DATA) noexcept {
		switch (DATA) {
			[[unlikely]]
			case 0b00000000:
				return;

			[[unlikely]]
			case 0b10000000:
				if (Quirk.wrap_sprites) { X &= (mDisplay.W - 1); }
				if (X < mDisplay.W) {
					if (!((m_display_buffer(X, Y) ^= 0x8) & 0x8))
						{ m_registers_V[0xF] = 1; }
				}
				return;

			[[likely]]
			default:
				if (Quirk.wrap_sprites) { X &= (mDisplay.W - 1); }
				else if (X >= mDisplay.W) { return; }

				for (auto B = 0; B < 8; ++B, ++X &= (mDisplay.W - 1)) {
					if (DATA & 0x80 >> B) {
						if (!((m_display_buffer(X, Y) ^= 0x8) & 0x8))
							{ m_registers_V[0xF] = 1; }
					}
					if (!Quirk.wrap_sprites && X == (mDisplay.W - 1)) { return; }
				}
				return;
		}
	}

	void SCHIP_MODERN::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		const auto pX = m_registers_V[X] & (mDisplay.W - 1);
		const auto pY = m_registers_V[Y] & (mDisplay.H - 1);

		m_registers_V[0xF] = 0;

		switch (N) {
			[[unlikely]]
			case 1:
				drawByte(pX, pY, m_memory_bank[m_register_I]);
				break;

			[[unlikely]]
			case 0:
				for (auto tN = 0, tY = pY; tN < 32;)
				{
					drawByte(pX + 0, tY, m_memory_bank[m_register_I + tN + 0]);
					drawByte(pX + 8, tY, m_memory_bank[m_register_I + tN + 1]);
					if (!Quirk.wrap_sprites && tY == (mDisplay.H - 1)) { break; }
					else { tN += 2; ++tY &= (mDisplay.H - 1); }
				}
				break;

			[[likely]]
			default:
				for (auto tN = 0, tY = pY; tN < N;)
				{
					drawByte(pX, tY, m_memory_bank[m_register_I + tN]);
					if (!Quirk.wrap_sprites && tY == (mDisplay.H - 1)) { break; }
					else { tN += 1; ++tY &= (mDisplay.H - 1); }
				}
				break;
		}

		if (Quirk.await_vblank) [[unlikely]] { trigger_interrupt(Interrupt::FRAME); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void SCHIP_MODERN::instruction_Ex9E(s32 X) noexcept {
		if (is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}
	void SCHIP_MODERN::instruction_ExA1(s32 X) noexcept {
		if (!is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void SCHIP_MODERN::instruction_Fx07(s32 X) noexcept {
		::assign_cast(m_registers_V[X], m_delay_timer);
	}
	void SCHIP_MODERN::instruction_Fx0A(s32 X) noexcept {
		m_key_reg_ref = &m_registers_V[X];
		trigger_interrupt(Interrupt::INPUT);
	}
	void SCHIP_MODERN::instruction_Fx15(s32 X) noexcept {
		::assign_cast(m_delay_timer, m_registers_V[X]);
	}
	void SCHIP_MODERN::instruction_Fx18(s32 X) noexcept {
		start_voice(m_registers_V[X] + (m_registers_V[X] == 1));
	}
	void SCHIP_MODERN::instruction_Fx1E(s32 X) noexcept {
		::assign_cast_add(m_register_I, m_registers_V[X]);
	}
	void SCHIP_MODERN::instruction_Fx29(s32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 5 + c_small_font_offset);
	}
	void SCHIP_MODERN::instruction_Fx30(s32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 10 + c_large_font_offset);
	}
	void SCHIP_MODERN::instruction_Fx33(s32 X) noexcept {
		const TriBCD bcd{ m_registers_V[X] };

		m_memory_bank[m_register_I + 0] = bcd.digit[2];
		m_memory_bank[m_register_I + 1] = bcd.digit[1];
		m_memory_bank[m_register_I + 2] = bcd.digit[0];
	}
	void SCHIP_MODERN::instruction_FN55(s32 N) noexcept {
		for (auto idx = 0; idx <= N; ++idx) { m_memory_bank[m_register_I + idx] = m_registers_V[idx]; }
		if (!Quirk.no_inc_i_reg) [[likely]] { ::assign_cast_add(m_register_I, N + 1); }
	}
	void SCHIP_MODERN::instruction_FN65(s32 N) noexcept {
		for (auto idx = 0; idx <= N; ++idx) { m_registers_V[idx] = m_memory_bank[m_register_I + idx]; }
		if (!Quirk.no_inc_i_reg) [[likely]] { ::assign_cast_add(m_register_I, N + 1); }
	}
	void SCHIP_MODERN::instruction_FN75(s32 N) noexcept {
		set_permaregs(N + 1);
	}
	void SCHIP_MODERN::instruction_FN85(s32 N) noexcept {
		get_permaregs(N + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
