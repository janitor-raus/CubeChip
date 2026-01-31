/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "MEGACHIP.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_MEGACHIP)

#include "BasicVideoSpec.hpp"
#include "CoreRegistry.hpp"

REGISTER_CORE(MEGACHIP, ".mc8")

/*==================================================================*/

void MEGACHIP::initialize_system() noexcept {
	copy_game_to_memory(m_memory_bank.data() + c_game_load_pos);
	copy_font_to_memory(m_memory_bank.data(), 180);

	set_base_system_framerate(c_sys_refresh_rate);

	m_voices[VOICE::UNIQUE].userdata = &m_audio_timers[VOICE::UNIQUE];
	m_voices[VOICE::BUZZER].userdata = &m_audio_timers[VOICE::BUZZER];

	m_current_pc = c_sys_boot_pos;

	set_display_properties(Resolution::LO);

	m_display_device.metadata_staging()
		.set_minimum_zoom(2)
		.set_inner_margin(4)
		.enabled = true;
}

void MEGACHIP::handle_cycle_loop() noexcept
	{ LOOP_DISPATCH(instruction_loop); }

template <typename Lambda>
void MEGACHIP::instruction_loop(Lambda&& condition) noexcept {
	for (m_cycle_count = 0; condition(); ++m_cycle_count) {
		const auto HI = m_memory_bank[m_current_pc++];
		const auto LO = m_memory_bank[m_current_pc++];

		#define _NNN ((HI << 8 | LO) & 0xFFF)
		#define _X (HI & 0xF)
		#define Y_ (LO >> 4)
		#define _N (LO & 0xF)

		switch (HI) {
			CASE_xNF(0x00):
				if (use_manual_vsync()) {
					switch (_NNN) {
						case 0x0010:
							instruction_0010();
							break;
						case 0x0700:
							instruction_0700();
							break;
						CASE_xNF(0x0600):
							instruction_060N(_N);
							break;
						CASE_xNF(0x0800):
							instruction_080N(_N);
							break;
						CASE_xNF(0x00B0):
							instruction_00BN(_N);
							break;
						CASE_xNF(0x00C0):
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
						default:
							switch (_X) {
								case 0x01:
									instruction_01NN(LO);
									break;
								case 0x02:
									instruction_02NN(LO);
									break;
								case 0x03:
									instruction_03NN(LO);
									break;
								case 0x04:
									instruction_04NN(LO);
									break;
								case 0x05:
									instruction_05NN(LO);
									break;
								case 0x09:
									instruction_09NN(LO);
									break;
								[[unlikely]]
								default: instruction_error(HI, LO);
							}
					}
				}
				else {
					if (_X) [[unlikely]] {
						instruction_error(HI, LO);
					} else {
						switch (LO) {
							case 0x11:
								instruction_0011();
								break;
							CASE_xNF0(0xB0):
								instruction_00BN(_N);
								break;
							CASE_xNF0(0xC0):
								instruction_00CN(_N);
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
					}
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

void MEGACHIP::push_audio_data() {
	if (use_manual_vsync()) {
		mix_audio_data({
			{ make_stream_wave, &m_voices[VOICE::UNIQUE] },
			{ make_pulse_wave,  &m_voices[VOICE::BUZZER] },
		});

		m_display_device.metadata_staging().set_border_color_if(
			!!m_audio_timers[VOICE::BUZZER], s_bit_colors[1]);
	}
	else {
		mix_audio_data({
			{ make_pulse_wave, &m_voices[VOICE::ID_0] },
			{ make_pulse_wave, &m_voices[VOICE::ID_1] },
			{ make_pulse_wave, &m_voices[VOICE::ID_2] },
			{ make_pulse_wave, &m_voices[VOICE::BUZZER] },
		});

		m_display_device.metadata_staging().set_border_color_if(
			!!::accumulate(m_audio_timers), s_bit_colors[1]);
	}
}

void MEGACHIP::push_video_data() {
	if (!use_manual_vsync()) {
		auto calc_color = use_pixel_trails()
			? [](u8 pixel) { return RGBA::premul(s_bit_colors[pixel != 0], c_bit_weight[pixel]); }
			: [](u8 pixel) { return s_bit_colors[pixel >> 3]; };

		for (auto i = 0u; i < m_display_buffer.size(); ++i) {
			auto pixel = m_display_buffer[i];
			auto color = calc_color(pixel);

			auto x = (i % cDisplayW_C8) * 2;
			auto y = (i / cDisplayW_C8) * 2;

			m_background_buffer(x + 0, y + 0) = color;
			m_background_buffer(x + 1, y + 0) = color;
			m_background_buffer(x + 0, y + 1) = color;
			m_background_buffer(x + 1, y + 1) = color;
		}

		flush_all_video_buffers(false, false);
	}
}

void MEGACHIP::set_display_properties(Resolution mode) {
	use_manual_vsync(mode == Resolution::MC);

	if (use_manual_vsync()) {
		m_display_device.metadata_staging()
			.set_viewport(cDisplayW_M8, cDisplayH_M8)
			.set_texture_tint(RGBA::Black);
		Quirk.await_vblank = false;
		m_target_cpf = cInstSpeedMC;
	}
	else {
		m_display_device.metadata_staging()
			.set_viewport(cDisplayW_M8, cDisplayW_M8 >> 1)
			.set_texture_tint(c_bit_colors[0]);

		use_hires_screen(mode != Resolution::LO);

		Quirk.await_vblank = !use_hires_screen();
		m_target_cpf = use_hires_screen() ? c_sys_speed_lo : c_sys_speed_hi;
	}
};

/*==================================================================*/

void MEGACHIP::skip_instruction() noexcept {
	m_current_pc += m_memory_bank[m_current_pc] == 0x01 ? 4 : 2;
}

void MEGACHIP::scroll_display_up(s32 N) {
	m_display_buffer.shift(0, -N);
}
void MEGACHIP::scroll_display_dn(s32 N) {
	m_display_buffer.shift(0, +N);
}
void MEGACHIP::scroll_display_lt() {
	m_display_buffer.shift(-4, 0);
}
void MEGACHIP::scroll_display_rt() {
	m_display_buffer.shift(+4, 0);
}

/*==================================================================*/

void MEGACHIP::init_font_sprite_colors() noexcept {
	for (auto i = 0; i < 10; ++i) {
		const auto mult = u8(255 - 11 * i);

		m_font_colors[i] = RGBA{
			ez::fixed_scale8(mult, 264),
			ez::fixed_scale8(mult, 291),
			ez::fixed_scale8(mult, 309),
		};
	}
}

void MEGACHIP::set_blend_callable(s32 mode) noexcept {
	switch (mode) {
		case BlendMode::LINEAR_DODGE:
			m_blend_callable = RGBA::Blend::LinearDodge;
			break;

		case BlendMode::MULTIPLY:
			m_blend_callable = RGBA::Blend::Multiply;
			break;

		default:
		case BlendMode::ALPHA_BLEND:
			m_blend_callable = RGBA::Blend::None;
			break;
	}
}

void MEGACHIP::scrap_all_video_buffers() noexcept {
	m_old_render_buffer.initialize();
	m_background_buffer.initialize();
	m_collision_map.initialize();
}

void MEGACHIP::flush_all_video_buffers(bool by_blending, bool and_advance) noexcept {
	m_display_device.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = ++m_display_device.metadata_staging();
		if (by_blending) {
			frame.copy_from(m_old_render_buffer, m_background_buffer, RGBA::alpha_blend);
		} else {
			frame.copy_from(m_background_buffer);
		}
	});

	if (and_advance) {
		m_old_render_buffer = m_background_buffer;
		m_background_buffer.initialize();
		m_collision_map.initialize();
	}
}

void MEGACHIP::start_audio_track(bool repeat) noexcept {
	if (auto* stream = m_audio_device.at(STREAM::MAIN)) {

		m_track.loop = repeat;
		m_track.data = &m_memory_bank[m_register_I + 6];
		m_track.size = m_memory_bank[m_register_I + 2] << 16
					 | m_memory_bank[m_register_I + 3] <<  8
					 | m_memory_bank[m_register_I + 4];

		const bool oob = m_track.data + m_track.size > &m_memory_bank.back();
		if (!m_track.size || oob) { m_track.reset(); }
		else {
			m_voices[VOICE::UNIQUE].set_phase(0.0).set_step(get_framerate_multiplier() * (
				(m_memory_bank[m_register_I + 0] << 8 | m_memory_bank[m_register_I + 1]) \
				/ f64(m_track.size) / stream->get_freq())).userdata = &m_track;
		}
	}
}

void MEGACHIP::make_stream_wave(f32* data, u32 size, Voice* voice, Stream*) noexcept {
	if (!voice || !voice->userdata) [[unlikely]] { return; }
	if (auto* track = static_cast<TrackData*>(voice->userdata)) {
		if (!track->enabled()) { return; }

		for (auto i = 0u; i < size; ++i) {
			const auto head = voice->peek_raw_phase(i);
			if (!track->loop && head >= 1.0) {
				track->reset(); return;
			} else {
				::assign_cast_add(data[i], (1.0 / 128) * \
					track->pos(head));
			}
		}
		voice->step_phase(size);
	}
}

void MEGACHIP::scroll_buffers_up(s32 N) {
	m_old_render_buffer.shift(0, -N);
	flush_all_video_buffers(true, false);
}
void MEGACHIP::scroll_buffers_dn(s32 N) {
	m_old_render_buffer.shift(0, +N);
	flush_all_video_buffers(true, false);
}
void MEGACHIP::scroll_buffers_lt() {
	m_old_render_buffer.shift(-4, 0);
	flush_all_video_buffers(true, false);
}
void MEGACHIP::scroll_buffers_rt() {
	m_old_render_buffer.shift(+4, 0);
	flush_all_video_buffers(true, false);
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void MEGACHIP::instruction_00BN(s32 N) noexcept {
		if (use_manual_vsync()) {
			scroll_buffers_up(N);
		} else {
			scroll_display_up(N);
		}
	}
	void MEGACHIP::instruction_00CN(s32 N) noexcept {
		if (use_manual_vsync()) {
			scroll_buffers_dn(N);
		} else {
			scroll_display_dn(N);
		}
	}
	void MEGACHIP::instruction_00E0() noexcept {
		if (use_manual_vsync()) {
			flush_all_video_buffers(true, true);
		} else {
			m_display_buffer.initialize();
		}
		trigger_interrupt(Interrupt::FRAME);
	}
	void MEGACHIP::instruction_00EE() noexcept {
		m_current_pc = m_stack_bank[--m_stack_head & 0xF];
	}
	void MEGACHIP::instruction_00FB() noexcept {
		if (use_manual_vsync()) {
			scroll_buffers_rt();
		} else {
			scroll_display_rt();
		}
	}
	void MEGACHIP::instruction_00FC() noexcept {
		if (use_manual_vsync()) {
			scroll_buffers_lt();
		} else {
			scroll_display_lt();
		}
	}
	void MEGACHIP::instruction_00FD() noexcept {
		trigger_interrupt(Interrupt::SOUND);
	}
	void MEGACHIP::instruction_00FE() noexcept {
		set_display_properties(Resolution::LO);
		trigger_interrupt(Interrupt::FRAME);
	}
	void MEGACHIP::instruction_00FF() noexcept {
		set_display_properties(Resolution::HI);
		trigger_interrupt(Interrupt::FRAME);
	}

	void MEGACHIP::instruction_0010() noexcept {
		set_display_properties(Resolution::LO);
		scrap_all_video_buffers();
		trigger_interrupt(Interrupt::FRAME);
	}
	void MEGACHIP::instruction_0011() noexcept {
		set_display_properties(Resolution::MC);

		set_blend_callable(BlendMode::ALPHA_BLEND);
		init_font_sprite_colors();
		scrap_all_video_buffers();

		m_texture.reset();
		m_track.reset();

		trigger_interrupt(Interrupt::FRAME);
	}
	void MEGACHIP::instruction_01NN(s32 NN) noexcept {
		::assign_cast(m_register_I, (NN << 16) | NNNN());
		::assign_cast_add(m_current_pc, 2);
	}
	void MEGACHIP::instruction_02NN(s32 NN) noexcept {
		for (auto pos = 0, byte = 0; pos < NN; byte += 4) {
			m_color_palette(++pos) = {
				m_memory_bank[m_register_I + byte + 1],
				m_memory_bank[m_register_I + byte + 2],
				m_memory_bank[m_register_I + byte + 3],
				m_memory_bank[m_register_I + byte + 0],
			};
		}
	}
	void MEGACHIP::instruction_03NN(s32 NN) noexcept {
		m_texture.w = NN ? NN : 256;
	}
	void MEGACHIP::instruction_04NN(s32 NN) noexcept {
		m_texture.h = NN ? NN : 256;
	}
	void MEGACHIP::instruction_05NN(s32 NN) noexcept {
		m_display_device.metadata_staging()
			.rmw_texture_tint().set_A(NN & 0xFF);
	}
	void MEGACHIP::instruction_060N(s32 N) noexcept {
		start_audio_track(N == 0);
	}
	void MEGACHIP::instruction_0700() noexcept {
		m_track.reset();
	}
	void MEGACHIP::instruction_080N(s32 N) noexcept {
		static constexpr u8 opacity[]{ 0xFF, 0x3F, 0x7F, 0xBF };
		::assign_cast(m_texture.opacity, opacity[N > 3 ? 0 : N]);
		set_blend_callable(N);
	}
	void MEGACHIP::instruction_09NN(s32 NN) noexcept {
		m_texture.collide = NN;
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void MEGACHIP::instruction_1NNN(s32 NNN) noexcept {
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void MEGACHIP::instruction_2NNN(s32 NNN) noexcept {
		m_stack_bank[m_stack_head++ & 0xF] = m_current_pc;
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void MEGACHIP::instruction_3xNN(s32 X, s32 NN) noexcept {
		if (m_registers_V[X] == NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void MEGACHIP::instruction_4xNN(s32 X, s32 NN) noexcept {
		if (m_registers_V[X] != NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void MEGACHIP::instruction_5xy0(s32 X, s32 Y) noexcept {
		if (m_registers_V[X] == m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void MEGACHIP::instruction_6xNN(s32 X, s32 NN) noexcept {
		::assign_cast(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void MEGACHIP::instruction_7xNN(s32 X, s32 NN) noexcept {
		::assign_cast_add(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void MEGACHIP::instruction_8xy0(s32 X, s32 Y) noexcept {
		::assign_cast(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy1(s32 X, s32 Y) noexcept {
		::assign_cast_or(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy2(s32 X, s32 Y) noexcept {
		::assign_cast_and(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy3(s32 X, s32 Y) noexcept {
		::assign_cast_xor(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy4(s32 X, s32 Y) noexcept {
		const auto sum = m_registers_V[X] + m_registers_V[Y];
		::assign_cast(m_registers_V[X], sum);
		::assign_cast(m_registers_V[0xF], sum >> 8);
	}
	void MEGACHIP::instruction_8xy5(s32 X, s32 Y) noexcept {
		const bool nborrow = m_registers_V[X] >= m_registers_V[Y];
		::assign_cast_sub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void MEGACHIP::instruction_8xy7(s32 X, s32 Y) noexcept {
		const bool nborrow = m_registers_V[Y] >= m_registers_V[X];
		::assign_cast_rsub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void MEGACHIP::instruction_8xy6(s32 X, s32  ) noexcept {
		const bool lsb = (m_registers_V[X] & 0x01) != 0;
		::assign_cast_shr(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], lsb);
	}
	void MEGACHIP::instruction_8xyE(s32 X, s32  ) noexcept {
		const bool msb = (m_registers_V[X] & 0x80) != 0;
		::assign_cast_shl(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void MEGACHIP::instruction_9xy0(s32 X, s32 Y) noexcept {
		if (m_registers_V[X] != m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void MEGACHIP::instruction_ANNN(s32 NNN) noexcept {
		::assign_cast(m_register_I, NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void MEGACHIP::instruction_BXNN(s32 X, s32 NNN) noexcept {
		jump_program_to(NNN + m_registers_V[X]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void MEGACHIP::instruction_CxNN(s32 X, s32 NN) noexcept {
		::assign_cast(m_registers_V[X], RNG->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	bool MEGACHIP::drawSingleBytes(
		s32 originX, s32 originY,
		s32 WIDTH,   s32 DATA
	) noexcept {
		if (!DATA) { return false; }
		bool collided = false;

		for (auto B = 0; B < WIDTH; ++B) {
			const auto offsetX = originX + B;

			if (DATA >> (WIDTH - 1 - B) & 0x1) {
				auto& pixel = m_display_buffer(offsetX, originY);
				if (!((pixel ^= 0x8) & 0x8)) { collided = true; }
			}
			if (offsetX == cDisplayW_C8 - 1) { return collided; }
		}
		return collided;
	}

	bool MEGACHIP::drawDoubleBytes(
		s32 originX, s32 originY,
		s32 WIDTH,   s32 DATA
	) noexcept {
		if (!DATA) { return false; }
		bool collided = false;

		for (auto B = 0; B < WIDTH; ++B) {
			const auto offsetX = originX + B;

			auto& pixelHI = m_display_buffer(offsetX, originY + 0);
			auto& pixelLO = m_display_buffer(offsetX, originY + 1);

			if (DATA >> (WIDTH - 1 - B) & 0x1) {
				collided |= !!(pixelHI & 0x8);
				pixelLO = pixelHI ^= 0x8;
			} else {
				pixelLO = pixelHI;
			}
			if (offsetX == cDisplayW_C8 - 1) { return collided; }
		}
		return collided;
	}

	void MEGACHIP::instruction_DxyN(s32 X, s32 Y, s32 N) noexcept {
		if (use_manual_vsync()) {
			const auto originX = m_registers_V[X] + 0;
			const auto originY = m_registers_V[Y] + 0;

			m_registers_V[0xF] = 0;

			if (!Quirk.wrap_sprites && originY >= cDisplayH_M8) { return; }
			if (m_texture.font_pos != m_register_I) [[likely]] { goto paintTexture; }

			for (auto rowN = 0, offsetY = originY; rowN < N; ++rowN)
			{
				if (Quirk.wrap_sprites && offsetY >= cDisplayH_M8) { continue; }
				const auto octoPixelBatch = m_memory_bank[m_register_I + rowN];

				for (auto colN = 7, offsetX = originX; colN >= 0; --colN)
				{
					if (octoPixelBatch >> colN & 0x1) {
						m_background_buffer(offsetX, offsetY) = m_font_colors[rowN];
					}

					if (!Quirk.wrap_sprites && offsetX == (cDisplayW_M8 - 1))
						{ break; } else { ++offsetX &= (cDisplayW_M8 - 1); }
				}
				if (!Quirk.wrap_sprites && offsetY == (cDisplayW_M8 - 1))
					{ break; } else { ++offsetY &= (cDisplayW_M8 - 1); }
			}
			return;

		paintTexture:
			if (m_register_I + m_texture.w * m_texture.h >= c_sys_memory_size)
				[[unlikely]] { m_texture.reset(); return; }

			for (auto rowN = 0, offsetY = originY; rowN < m_texture.h; ++rowN)
			{
				if (Quirk.wrap_sprites && offsetY >= cDisplayH_M8) { continue; }
				const auto offsetI = rowN * m_texture.w;

				for (auto colN = 0, offsetX = originX; colN < m_texture.w; ++colN)
				{
					if (const auto sourceColorIdx = m_memory_bank[m_register_I + offsetI + colN])
					{
						auto& collideCoord = m_collision_map(offsetX, offsetY);
						auto& backbufCoord = m_background_buffer(offsetX, offsetY);

						if (collideCoord == m_texture.collide)
							[[unlikely]] { m_registers_V[0xF] = 1; }

						collideCoord = sourceColorIdx;
						backbufCoord = RGBA::composite_blend(m_color_palette(sourceColorIdx), \
							backbufCoord, m_blend_callable, u8(m_texture.opacity));
					}
					if (!Quirk.wrap_sprites && offsetX == (cDisplayW_M8 - 1))
						{ break; } else { ++offsetX &= (cDisplayW_M8 - 1); }
				}
				if (!Quirk.wrap_sprites && offsetY == (cDisplayH_M8 - 1))
					{ break; } else { ++offsetY %= cDisplayH_M8; }
			}
		} else {
			if (use_hires_screen()) {
				const auto offsetX = 8 - (m_registers_V[X] & 7);
				const auto originX = m_registers_V[X] & 0x78;
				const auto originY = m_registers_V[Y] & 0x3F;

				auto collisions = 0;

				if (N == 0) {
					for (auto rowN = 0; rowN < 16; ++rowN) {
						const auto offsetY = originY + rowN;

						collisions += drawSingleBytes(originX, offsetY, offsetX ? 24 : 16, (
							m_memory_bank[m_register_I + 2 * rowN + 0] << 8 |
							m_memory_bank[m_register_I + 2 * rowN + 1] << 0
						) << offsetX);

						if (offsetY == (cDisplayH_C8 - 1)) { break; }
					}
				} else {
					for (auto rowN = 0; rowN < N; ++rowN) {
						const auto offsetY = originY + rowN;

						collisions += drawSingleBytes(originX, offsetY, offsetX ? 16 : 8,
							m_memory_bank[m_register_I + rowN] << offsetX);

						if (offsetY == (cDisplayH_C8 - 1)) { break; }
					}
				}
				::assign_cast(m_registers_V[0xF], collisions);
			}
			else {
				const auto offsetX = 16 - 2 * (m_registers_V[X] & 0x07);
				const auto originX = m_registers_V[X] * 2 & 0x70;
				const auto originY = m_registers_V[Y] * 2 & 0x3F;
				const auto lengthN = N == 0 ? 16 : N;

				auto collisions = 0;

				for (auto rowN = 0; rowN < lengthN; ++rowN) {
					const auto offsetY = originY + rowN * 2;

					collisions += drawDoubleBytes(originX, offsetY, 0x20,
						ez::bit_dup8(m_memory_bank[m_register_I + rowN]) << offsetX);

					if (offsetY == (cDisplayH_C8 - 2)) { break; }
				}
				::assign_cast(m_registers_V[0xF], collisions != 0);
			}
		}

		if (Quirk.await_vblank) [[unlikely]] { trigger_interrupt(Interrupt::FRAME); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void MEGACHIP::instruction_Ex9E(s32 X) noexcept {
		if (is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}
	void MEGACHIP::instruction_ExA1(s32 X) noexcept {
		if (!is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void MEGACHIP::instruction_Fx07(s32 X) noexcept {
		::assign_cast(m_registers_V[X], m_delay_timer);
	}
	void MEGACHIP::instruction_Fx0A(s32 X) noexcept {
		if (use_manual_vsync()) [[unlikely]] {
			flush_all_video_buffers(false, false);
		}
		m_key_reg_ref = &m_registers_V[X];
		trigger_interrupt(Interrupt::INPUT);
	}
	void MEGACHIP::instruction_Fx15(s32 X) noexcept {
		::assign_cast(m_delay_timer, m_registers_V[X]);
	}
	void MEGACHIP::instruction_Fx18(s32 X) noexcept {
		start_voice_at(VOICE::BUZZER, m_registers_V[X] + (m_registers_V[X] == 1));
	}
	void MEGACHIP::instruction_Fx1E(s32 X) noexcept {
		::assign_cast_add(m_register_I, m_registers_V[X]);
	}
	void MEGACHIP::instruction_Fx29(s32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 5 + c_small_font_offset);
		::assign_cast(m_texture.font_pos, m_register_I);
	}
	void MEGACHIP::instruction_Fx30(s32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 10 + c_large_font_offset);
		::assign_cast(m_texture.font_pos, m_register_I);
	}
	void MEGACHIP::instruction_Fx33(s32 X) noexcept {
		const TriBCD bcd{ m_registers_V[X] };

		m_memory_bank[m_register_I + 0] = bcd.digit[2];
		m_memory_bank[m_register_I + 1] = bcd.digit[1];
		m_memory_bank[m_register_I + 2] = bcd.digit[0];
	}
	void MEGACHIP::instruction_FN55(s32 N) noexcept {
		for (auto idx = 0; idx <= N; ++idx) {
			m_memory_bank[m_register_I + idx] = m_registers_V[idx];
		}
	}
	void MEGACHIP::instruction_FN65(s32 N) noexcept {
		for (auto idx = 0; idx <= N; ++idx) {
			m_registers_V[idx] = m_memory_bank[m_register_I + idx];
		}
	}
	void MEGACHIP::instruction_FN75(s32 N) noexcept {
		set_permaregs(std::min(N, 7) + 1);
	}
	void MEGACHIP::instruction_FN85(s32 N) noexcept {
		get_permaregs(std::min(N, 7) + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
