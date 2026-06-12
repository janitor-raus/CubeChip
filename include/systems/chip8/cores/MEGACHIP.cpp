/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "MEGACHIP.hpp"
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_MEGACHIP)

#include "CoreRegistry.inl"

REGISTER_SYSTEM_CORE(MEGACHIP)

/*==================================================================*/

void MEGACHIP::initialize_system() noexcept {
	copy_file_image_to(m_memory, c_game_load_pos);
	copy_font_data_to(m_memory, 180);

	m_base_system_framerate = c_sys_refresh_rate;

	m_memory_editor.set_memory_range(m_memory.data(), m_memory.size());

	m_current_pc = c_sys_boot_pos;

	set_display_properties(Resolution::LO);

	m_display_device.metadata().edit([](auto& meta) noexcept {
		meta.minimum_zoom = 2;
		meta.inner_margin = 4;
		meta.enabled = true;
	});
}

void MEGACHIP::instruction_loop() noexcept {
	const auto target_cpf = has_cached_system_state(EmuState::BENCH)
		&& m_debugger_cpf ? m_debugger_cpf : m_standard_cpf;
	for (m_cycle_count = 0; m_interrupt == Interrupt::CLEAR
		&& m_cycle_count < target_cpf; ++m_cycle_count)
	{
		const auto HI = m_memory[m_current_pc++];
		const auto LO = m_memory[m_current_pc++];

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

void MEGACHIP::push_audio_data() noexcept {
	if (use_manual_vsync()) {
		mix_audio_data(
			[&](auto buffer) noexcept { make_stream_wave(buffer, m_voices[VOICE::UNIQUE], m_track); },
			[&](auto buffer) noexcept { make_pulse_wave(buffer, m_voices[VOICE::BUZZER]); }
		);

		if (has_cached_system_state(EmuState::ANY_PAUSE)) { return; }
		m_display_device.metadata().edit([&](auto& meta) noexcept {
			meta.set_border_color_if(!!m_voices[VOICE::BUZZER].timer, s_bit_colors[1]);
		});
	}
	else {
		mix_audio_data(
			[&](auto buffer) noexcept { make_pulse_wave(buffer, m_voices[VOICE::ID_0]); },
			[&](auto buffer) noexcept { make_pulse_wave(buffer, m_voices[VOICE::ID_1]); },
			[&](auto buffer) noexcept { make_pulse_wave(buffer, m_voices[VOICE::ID_2]); },
			[&](auto buffer) noexcept { make_pulse_wave(buffer, m_voices[VOICE::BUZZER]); }
		);

		if (has_cached_system_state(EmuState::ANY_PAUSE)) { return; }
		m_display_device.metadata().edit([&](auto& meta) noexcept {
			meta.set_border_color_if(!!::accumulate(m_voices, 0), s_bit_colors[1]);
		});
	}
}

void MEGACHIP::push_video_data() noexcept {
	if (use_manual_vsync()) {
		if (m_interrupt == Interrupt::INPUT) {
			// During the INPUT interrupt, we want to keep the swapchain
			// alive with updates, even if the background map is stale.
			flush_all_video_buffers(false, false);
		}
	} else {
		auto calc_color = use_pixel_trails()
			? [](u8 pixel) { return RGBA::premul(s_bit_colors[pixel != 0], c_bit_weight[pixel]); }
			: [](u8 pixel) { return s_bit_colors[pixel >> 3]; };

		for (auto i = 0u; i < m_display_map.size(); ++i) {
			auto color = calc_color(m_display_map[i]);

			auto x = (i % (c_sys_screen_W/2)) * 2;
			auto y = (i / (c_sys_screen_W/2)) * 2;

			m_background_map(x + 0, y + 0) = color;
			m_background_map(x + 1, y + 0) = color;
			m_background_map(x + 0, y + 1) = color;
			m_background_map(x + 1, y + 1) = color;
		}

		flush_all_video_buffers(false, false);
	}
}

void MEGACHIP::set_display_properties(Resolution mode) noexcept {
	use_manual_vsync(mode == Resolution::MC);

	m_display_device.metadata().edit(
	[&](auto& meta) noexcept {
		if (use_manual_vsync()) {
			meta.set_viewport(c_sys_screen_W, c_sys_screen_H);
			meta.texture_tint = RGBA::Black;

			sub_quirk(AWAIT_VBLANK);
			m_standard_cpf = c_sys_speed_lo * 100;
		}
		else {
			meta.set_viewport(c_sys_screen_W, c_sys_screen_W/2);
			meta.texture_tint = c_bit_colors[0];

			if (mode == Resolution::LO) {
				use_hires_screen(false);
				add_quirk(AWAIT_VBLANK);
				m_standard_cpf = c_sys_speed_hi;
			} else {
				use_hires_screen(true);
				sub_quirk(AWAIT_VBLANK);
				m_standard_cpf = c_sys_speed_lo;
			}
		}
	});
};

/*==================================================================*/

void MEGACHIP::skip_instruction() noexcept {
	m_current_pc += m_memory[m_current_pc] == 0x01 ? 4 : 2;
}

void MEGACHIP::scroll_display_up(u32 N) noexcept {
	m_display_map.shift(0, -s32(N));
}
void MEGACHIP::scroll_display_dn(u32 N) noexcept {
	m_display_map.shift(0, +s32(N));
}
void MEGACHIP::scroll_display_lt() noexcept {
	m_display_map.shift(-4, 0);
}
void MEGACHIP::scroll_display_rt() noexcept {
	m_display_map.shift(+4, 0);
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

void MEGACHIP::set_blend_callable(u32 mode) noexcept {
	switch (mode) {
		case BlendMode::LINEAR_DODGE:
			m_blend_mode = BlendMode::LINEAR_DODGE;
			break;

		case BlendMode::MULTIPLY:
			m_blend_mode = BlendMode::MULTIPLY;
			break;

		default:
		case BlendMode::ALPHA_BLEND:
			m_blend_mode = BlendMode::ALPHA_BLEND;
			break;
	}
}

void MEGACHIP::scrap_all_video_buffers() noexcept {
	m_old_render_map.fill();
	m_background_map.fill();
	m_collision_map.fill();
}

void MEGACHIP::flush_all_video_buffers(bool by_blending, bool and_advance) noexcept {
	m_display_device.swapchain().acquire([&](auto& frame) noexcept {
		frame.metadata = m_display_device.metadata().copy();
		if (by_blending) {
			frame.copy_from(m_old_render_map, m_background_map, RGBA::alpha_blend);
		} else {
			frame.copy_from(m_background_map);
		}
	});

	if (and_advance) {
		std::copy(EXEC_POLICY(unseq)
			m_background_map.begin(), m_background_map.end(),
			m_old_render_map.begin()
		);
		m_background_map.fill();
		m_collision_map.fill();
	}
}

void MEGACHIP::start_audio_track(bool repeat) noexcept {
	if (m_audio_device) {
		auto* track_src = &m_memory[m_register_I];

		m_track.loop = repeat;
		m_track.data = track_src + 6;
		m_track.size = track_src[2] << 16
					 | track_src[3] <<  8
					 | track_src[4];

		const bool oob = m_track.data + m_track.size > &m_memory.back();
		if (!m_track.size || oob) { m_track.reset(); }
		else {
			m_voices[VOICE::UNIQUE].set_phase(0.0).set_step(
				(track_src[0] << 8 | track_src[1]) \
				/ f64(m_track.size) / m_audio_device.get_freq());
		}
	}
}

void MEGACHIP::make_stream_wave(SampleBuffer buffer, Voice& voice, TrackData& track) noexcept {
	if (const auto sample_count = u32(buffer.size() * track.enabled())) {
		for (auto i = 0u; i < sample_count; ++i) {
			const auto head = voice.peek_raw_phase(i);
			if (!track.loop && head >= 1.0) {
				track.reset(); return;
			} else {
				::assign_cast_add(buffer[i],
					(1.0 / 128) * track.pos(head));
			}
		}
		voice.step_phase(sample_count);
	}
}

void MEGACHIP::scroll_buffers_up(u32 N) noexcept {
	m_old_render_map.shift(0, -s32(N));
	flush_all_video_buffers(true, false);
}
void MEGACHIP::scroll_buffers_dn(u32 N) noexcept {
	m_old_render_map.shift(0, +s32(N));
	flush_all_video_buffers(true, false);
}
void MEGACHIP::scroll_buffers_lt() noexcept {
	m_old_render_map.shift(-4, 0);
	flush_all_video_buffers(true, false);
}
void MEGACHIP::scroll_buffers_rt() noexcept {
	m_old_render_map.shift(+4, 0);
	flush_all_video_buffers(true, false);
}

/*==================================================================*/
	#pragma region 0 instruction branch

	void MEGACHIP::instruction_00BN(u32 N) noexcept {
		if (use_manual_vsync()) {
			scroll_buffers_up(N);
		} else {
			scroll_display_up(N);
		}
	}
	void MEGACHIP::instruction_00CN(u32 N) noexcept {
		if (use_manual_vsync()) {
			scroll_buffers_dn(N);
		} else {
			scroll_display_dn(N);
		}
	}
	void MEGACHIP::instruction_00E0() noexcept {
		if (use_manual_vsync()) {
			flush_all_video_buffers(false, true);
		} else {
			m_display_map.fill();
		}
		trigger_interrupt(Interrupt::FRAME);
	}
	void MEGACHIP::instruction_00EE() noexcept {
		m_current_pc = m_stack.pop();
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
	void MEGACHIP::instruction_01NN(u32 NN) noexcept {
		::assign_cast(m_register_I, (NN << 16) | NNNN());
		::assign_cast_add(m_current_pc, 2);
	}
	void MEGACHIP::instruction_02NN(u32 NN) noexcept {
		auto* src = &m_memory[m_register_I];
		for (auto pos = 0u; pos < NN; src += 4) {
			m_color_palette[++pos] = { src[1], src[2], src[3], src[0] };
		}
	}
	void MEGACHIP::instruction_03NN(u32 NN) noexcept {
		m_texture.w = NN ? NN : 256u;
	}
	void MEGACHIP::instruction_04NN(u32 NN) noexcept {
		m_texture.h = NN ? NN : 256u;
	}
	void MEGACHIP::instruction_05NN(u32 NN) noexcept {
		m_display_device.metadata().edit([&](auto& meta) noexcept {
			meta.texture_tint.set_A(NN & 0xFF);
		});
	}
	void MEGACHIP::instruction_060N(u32 N) noexcept {
		start_audio_track(N == 0u);
	}
	void MEGACHIP::instruction_0700() noexcept {
		m_track.reset();
	}
	void MEGACHIP::instruction_080N(u32 N) noexcept {
		static constexpr u8 opacity[]{ 0xFF, 0x3F, 0x7F, 0xBF };
		::assign_cast(m_texture.opacity, opacity[N > 3 ? 0 : N]);
		set_blend_callable(N);
	}
	void MEGACHIP::instruction_09NN(u32 NN) noexcept {
		m_texture.collide = NN;
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	void MEGACHIP::instruction_1NNN(u32 NNN) noexcept {
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	void MEGACHIP::instruction_2NNN(u32 NNN) noexcept {
		m_stack.push(m_current_pc);
		jump_program_to(NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	void MEGACHIP::instruction_3xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] == NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	void MEGACHIP::instruction_4xNN(u32 X, u32 NN) noexcept {
		if (m_registers_V[X] != NN) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	void MEGACHIP::instruction_5xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] == m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	void MEGACHIP::instruction_6xNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	void MEGACHIP::instruction_7xNN(u32 X, u32 NN) noexcept {
		::assign_cast_add(m_registers_V[X], NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	void MEGACHIP::instruction_8xy0(u32 X, u32 Y) noexcept {
		::assign_cast(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy1(u32 X, u32 Y) noexcept {
		::assign_cast_or(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy2(u32 X, u32 Y) noexcept {
		::assign_cast_and(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy3(u32 X, u32 Y) noexcept {
		::assign_cast_xor(m_registers_V[X], m_registers_V[Y]);
	}
	void MEGACHIP::instruction_8xy4(u32 X, u32 Y) noexcept {
		const auto sum = m_registers_V[X] + m_registers_V[Y];
		::assign_cast(m_registers_V[X], sum);
		::assign_cast(m_registers_V[0xF], sum >> 8);
	}
	void MEGACHIP::instruction_8xy5(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[X] >= m_registers_V[Y];
		::assign_cast_sub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void MEGACHIP::instruction_8xy7(u32 X, u32 Y) noexcept {
		const bool nborrow = m_registers_V[Y] >= m_registers_V[X];
		::assign_cast_rsub(m_registers_V[X], m_registers_V[Y]);
		::assign_cast(m_registers_V[0xF], nborrow);
	}
	void MEGACHIP::instruction_8xy6(u32 X, u32  ) noexcept {
		const bool lsb = (m_registers_V[X] & 0x01) != 0;
		::assign_cast_shr(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], lsb);
	}
	void MEGACHIP::instruction_8xyE(u32 X, u32  ) noexcept {
		const bool msb = (m_registers_V[X] & 0x80) != 0;
		::assign_cast_shl(m_registers_V[X], 1);
		::assign_cast(m_registers_V[0xF], msb);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	void MEGACHIP::instruction_9xy0(u32 X, u32 Y) noexcept {
		if (m_registers_V[X] != m_registers_V[Y]) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	void MEGACHIP::instruction_ANNN(u32 NNN) noexcept {
		::assign_cast(m_register_I, NNN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	void MEGACHIP::instruction_BXNN(u32 X, u32 NNN) noexcept {
		jump_program_to(NNN + m_registers_V[X]);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	void MEGACHIP::instruction_CxNN(u32 X, u32 NN) noexcept {
		::assign_cast(m_registers_V[X], m_rng->next() & NN);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	bool MEGACHIP::draw_single_byte(
		u32 x_begin, u32 y_begin,
		u32 byte_w,  u32 data
	) noexcept {
		if (!data) { return false; }
		bool collided = false;

		const auto cols = std::min(byte_w, c_sys_screen_W - x_begin);
		auto* px_row = &m_display_map(x_begin, y_begin);

		for (auto col = 0u; col < cols; ++col) {
			if (data >> (byte_w - 1 - col) & 0x1) {
				collided |= !!(px_row[col] & 0x8);
				px_row[col] ^= 0x8;
			}
		}
		return collided;
	}

	bool MEGACHIP::draw_double_byte(
		u32 x_begin, u32 y_begin,
		u32 byte_w,  u32 data
	) noexcept {
		bool collided = false;

		const auto cols = std::min(byte_w, (c_sys_screen_W/2) - x_begin);
		auto* up_row = &m_display_map(x_begin, y_begin + 0);
		auto* dn_row = &m_display_map(x_begin, y_begin + 1);

		for (auto col = 0u; col < cols; ++col) {
			if (data >> (byte_w - 1 - col) & 0x1) {
				collided |= !!(up_row[col] & 0x8);
				dn_row[col] = up_row[col] ^= 0x8;
			} else {
				dn_row[col] = up_row[col];
			}
		}
		return collided;
	}

	template <IsBlendMode BlendMode, bool wrap_sprites>
	void MEGACHIP::draw_texture(u32 x_begin, u32 y_begin) noexcept {
		if (m_register_I + m_texture.w * m_texture.h >= c_sys_memory_size)
			[[unlikely]] { m_texture.reset(); return; }

		for (auto row = 0u, true_y = y_begin; row < m_texture.h; ++row) {
			if constexpr (wrap_sprites) {
				// MEGACHIP preserves Y progression into hidden 192..255 space
				// before wrapping back to 0. Drawing must continue to advance true_y
				// even if it's out of bounds, but skip drawing until it wraps.
				if (true_y >= c_sys_screen_H) { ++true_y &= (c_sys_screen_W - 1); continue; }
			}

			auto* collision_row = &m_collision_map(0, true_y);
			auto* bg_buffer_row = &m_background_map(0, true_y);
			auto* data_line_row = &m_memory[m_register_I + row * m_texture.w];

			for (auto col = 0u, true_x = x_begin; col < m_texture.w; ++col) {
				if (const auto src_color_idx = data_line_row[col]) {
					auto& collision_idx = collision_row[true_x];
					auto& bg_buffer_idx = bg_buffer_row[true_x];

					if (collision_idx == m_texture.collide)
						[[unlikely]] { m_registers_V[0xF] = 1; }

					collision_idx = src_color_idx;
					bg_buffer_idx = RGBA::composite_blend<BlendMode>(
						m_color_palette[src_color_idx],
						bg_buffer_idx, u8(m_texture.opacity)
					);
				}

				if constexpr (wrap_sprites) { ++true_x &= (c_sys_screen_W - 1); }
				else { if (++true_x == c_sys_screen_W) { break; } }
			}

			if constexpr (wrap_sprites) { ++true_y &= (c_sys_screen_W - 1); }
			else { if (++true_y == c_sys_screen_H) { break; } }
		}
	}

	template void MEGACHIP::draw_texture<RGBA::Blend::None,  true>(u32, u32) noexcept;
	template void MEGACHIP::draw_texture<RGBA::Blend::None, false>(u32, u32) noexcept;
	template void MEGACHIP::draw_texture<RGBA::Blend::LinearDodge,  true>(u32, u32) noexcept;
	template void MEGACHIP::draw_texture<RGBA::Blend::LinearDodge, false>(u32, u32) noexcept;
	template void MEGACHIP::draw_texture<RGBA::Blend::Multiply,  true>(u32, u32) noexcept;
	template void MEGACHIP::draw_texture<RGBA::Blend::Multiply, false>(u32, u32) noexcept;

	void MEGACHIP::instruction_DxyN(u32 X, u32 Y, u32 N) noexcept {
		if (use_manual_vsync()) {
			const auto x_begin = u32(m_registers_V[X]);
			const auto y_begin = u32(m_registers_V[Y]);

			m_registers_V[0xF] = 0;

			if (!has_quirk(WRAP_SPRITES) && y_begin >= c_sys_screen_H) { return; }
			if (m_texture.font_pos != m_register_I) [[likely]] { goto draw_texture_path; }

			for (auto row = 0u, true_y = y_begin; row < N; ++row) {
				if (has_quirk(WRAP_SPRITES) && true_y >= c_sys_screen_H) { continue; }
				const auto pixel_data = m_memory[m_register_I + row];

				for (auto col = 0u, true_x = x_begin; col < 8u; ++col)
				{
					if (pixel_data << col & 0x80) {
						m_background_map(true_x, true_y) = m_font_colors[row];
					}

					if (!has_quirk(WRAP_SPRITES) && true_x == (c_sys_screen_W - 1))
						{ break; } else { ++true_x &= (c_sys_screen_W - 1); }
				}
				if (!has_quirk(WRAP_SPRITES) && true_y == (c_sys_screen_W - 1))
					{ break; } else { ++true_y &= (c_sys_screen_W - 1); }
			}
			return;

		draw_texture_path:
			switch (m_blend_mode) {
				case BlendMode::LINEAR_DODGE:
					if (has_quirk(WRAP_SPRITES)) {
						draw_texture<RGBA::Blend::LinearDodge, true>(x_begin, y_begin);
					} else {
						draw_texture<RGBA::Blend::LinearDodge, false>(x_begin, y_begin);
					}
					break;

				case BlendMode::MULTIPLY:
					if (has_quirk(WRAP_SPRITES)) {
						draw_texture<RGBA::Blend::Multiply, true>(x_begin, y_begin);
					} else {
						draw_texture<RGBA::Blend::Multiply, false>(x_begin, y_begin);
					}
					break;

				default:
				case BlendMode::ALPHA_BLEND:
					if (has_quirk(WRAP_SPRITES)) {
						draw_texture<RGBA::Blend::None, true>(x_begin, y_begin);
					} else {
						draw_texture<RGBA::Blend::None, false>(x_begin, y_begin);
					}
					break;
			}
		} else {
			if (use_hires_screen()) {
				const auto x_shift = 8u - (m_registers_V[X] & 7);
				const auto x_begin = m_registers_V[X] & 0x78u;
				const auto y_begin = m_registers_V[Y] & 0x3Fu;

				auto collisions = 0u;

				if (N == 0u) {
					for (auto row = 0u, true_y = y_begin; row < 16u; ++row) {
						collisions += draw_single_byte(x_begin, true_y, x_shift ? 24 : 16, (
							m_memory[m_register_I + 2 * row + 0] << 8 |
							m_memory[m_register_I + 2 * row + 1] << 0
						) << x_shift);

						if (++true_y == (c_sys_screen_H/3)) { break; }
					}
				} else {
					for (auto row = 0u, true_y = y_begin; row < N; ++row) {
						collisions += draw_single_byte(x_begin, true_y, x_shift ? 16 : 8,
							m_memory[m_register_I + row] << x_shift);

						if (++true_y == (c_sys_screen_H/3)) { break; }
					}
				}
				::assign_cast(m_registers_V[0xF], collisions);
			}
			else {
				const auto x_shift = 16 - 2 * (m_registers_V[X] & 0x07u);
				const auto x_begin = m_registers_V[X] * 2 & 0x70u;
				const auto y_begin = m_registers_V[Y] * 2 & 0x3Eu;
				const auto n_count = N == 0u ? 16u : N;

				auto collisions = 0u;

				for (auto row = 0u, true_y = y_begin; row < n_count; ++row) {
					collisions += draw_double_byte(x_begin, true_y, 0x20,
						ez::bit_dup8(m_memory[m_register_I + row]) << x_shift);

					if ((true_y += 2) == (c_sys_screen_H/3)) { break; }
				}
				::assign_cast(m_registers_V[0xF], collisions != 0u);
			}
		}

		trigger_interrupt(Interrupt::FRAME, has_quirk(AWAIT_VBLANK));
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	void MEGACHIP::instruction_Ex9E(u32 X) noexcept {
		if (is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}
	void MEGACHIP::instruction_ExA1(u32 X) noexcept {
		if (!is_key_held_P1(m_registers_V[X])) { skip_instruction(); }
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	void MEGACHIP::instruction_Fx07(u32 X) noexcept {
		::assign_cast(m_registers_V[X], m_delay_timer);
	}
	void MEGACHIP::instruction_Fx0A(u32 X) noexcept {
		m_key_reg_ref = &m_registers_V[X];
		trigger_interrupt(Interrupt::INPUT);
	}
	void MEGACHIP::instruction_Fx15(u32 X) noexcept {
		::assign_cast(m_delay_timer, m_registers_V[X]);
	}
	void MEGACHIP::instruction_Fx18(u32 X) noexcept {
		start_voice_at(VOICE::BUZZER, m_registers_V[X] + (m_registers_V[X] == 1));
	}
	void MEGACHIP::instruction_Fx1E(u32 X) noexcept {
		::assign_cast_add(m_register_I, m_registers_V[X]);
	}
	void MEGACHIP::instruction_Fx29(u32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 5 + c_small_font_offset);
		::assign_cast(m_texture.font_pos, m_register_I);
	}
	void MEGACHIP::instruction_Fx30(u32 X) noexcept {
		::assign_cast(m_register_I, (m_registers_V[X] & 0xF) * 10 + c_large_font_offset);
		::assign_cast(m_texture.font_pos, m_register_I);
	}
	void MEGACHIP::instruction_Fx33(u32 X) noexcept {
		const TriBCD bcd{ m_registers_V[X] };

		m_memory[m_register_I + 0] = bcd.digit[2];
		m_memory[m_register_I + 1] = bcd.digit[1];
		m_memory[m_register_I + 2] = bcd.digit[0];
	}
	void MEGACHIP::instruction_FN55(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_memory[m_register_I + i] = m_registers_V[i]; }
	}
	void MEGACHIP::instruction_FN65(u32 N) noexcept {
		for (auto i = 0u; i <= N; ++i) { m_registers_V[i] = m_memory[m_register_I + i]; }
	}
	void MEGACHIP::instruction_FN75(u32 N) noexcept {
		set_permaregs(std::min(N, 7u) + 1);
	}
	void MEGACHIP::instruction_FN85(u32 N) noexcept {
		get_permaregs(std::min(N, 7u) + 1);
	}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

#endif
