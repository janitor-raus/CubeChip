/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#define ENABLE_CHIP8_SYSTEM
#ifdef ENABLE_CHIP8_SYSTEM

#include "../SystemInterface.hpp"

/*==================================================================*/

class Chip8_CoreInterface : public SystemInterface {

protected:
	enum STREAM : u32 { MAIN };
	enum VOICE : u32 {
		ID_0, ID_1, ID_2, ID_3, COUNT,
		BUZZER = ID_3, UNIQUE = ID_0,
	};

	static inline std::string s_permaregs_path{};
	static inline std::string s_savestate_path{};
	static constexpr f32 c_tonal_offset = 160.0f;

	struct TriBCD final {
		// 2 = hundreds | 1 = tens | 0 = ones
		u8 digit[3];

		constexpr TriBCD(u32 value) noexcept {
			::assign_cast(digit[2], value * 0x51EB851Full >> 37);
			::assign_cast(m_temp_val, value - digit[2] * 100);
			::assign_cast(digit[1], m_temp_val * 0xCCCDull >> 19);
			::assign_cast(digit[0], m_temp_val - digit[1] * 10);
		}

	private:
		u32 m_temp_val;
	};

/*==================================================================*/

	AudioDevice m_audio_device;

	std::array<Voice, VOICE::COUNT>
		m_voices{};

	std::array<AudioTimer, VOICE::COUNT>
		m_audio_timers{};

	void start_voice(u32 duration, u32 tone = 0) noexcept;
	void start_voice_at(u32 voice_index, u32 duration, u32 tone = 0) noexcept;

	void mix_audio_data(VoiceGenerators processors) noexcept;
	static void make_pulse_wave(f32* data, u32 size, Voice* voice, Stream* stream) noexcept;

/*==================================================================*/

	std::vector<SimpleKeyMapping> m_custom_binds;

private:
	u32  m_tick_last{};
	u32  m_tick_span{};

	u32  m_keys_this{}; // bitfield of key states in this frame
	u32  m_keys_last{}; // bitfield of key states in last frame
	u32  m_keys_hide{}; // bitfield of keys excluded from input checks
	u32  m_keys_loop{}; // bitfield of keys repeating input on Fx0A

protected:
	u8*  m_key_reg_ref{};

protected:
	void update_key_states() noexcept;
	void load_preset_binds() noexcept;

	template <IsContiguousContainer T>
		requires (SameValueTypes<T, decltype(m_custom_binds)>)
	void load_custom_binds(const T& binds) noexcept {
		m_custom_binds.assign(std::begin(binds), std::end(binds));
		m_keys_last = m_keys_this = m_keys_hide = 0;
	}

	bool catch_key_press(u8* keyReg) noexcept;
	bool is_key_held_P1(u32 keyIndex) const noexcept;
	bool is_key_held_P2(u32 keyIndex) const noexcept;

/*==================================================================*/

	DisplayDevice m_display_device;

/*==================================================================*/

	struct alignas(int) PlatformQuirks final {
		bool reset_vf_reg{};
		bool jump_with_vx{};
		bool shift_vx_reg{};
		bool no_inc_i_reg{};
		bool x1_inc_i_reg{};
		bool await_vblank{};
		bool await_scroll{};
		bool wrap_sprites{};
	} Quirk;

/*==================================================================*/

	struct alignas(int) PlatformTraits final {
		bool use_hires_screen{};
		bool use_manual_vsync{};
		bool use_pixel_trails{};
		bool : 8;
	} Trait;

	bool use_hires_screen()     const noexcept { return Trait.use_hires_screen; }
	bool use_manual_vsync()     const noexcept { return Trait.use_manual_vsync; }
	bool use_pixel_trails()     const noexcept { return Trait.use_pixel_trails; }
	bool use_hires_screen(bool state) noexcept { return std::exchange(Trait.use_hires_screen, state); }
	bool use_manual_vsync(bool state) noexcept { return std::exchange(Trait.use_manual_vsync, state); }
	bool use_pixel_trails(bool state) noexcept { return std::exchange(Trait.use_pixel_trails, state); }

/*==================================================================*/

	enum class Interrupt {
		CLEAR, // no interrupt
		FRAME, // single-frame
		SOUND, // wait for sound and stop
		DELAY, // wait for delay and proceed
		INPUT, // wait for input and proceed
		WAIT1, // intermediate wait state towards FINAL
		FINAL, // end state, all is well
		ERROR, // end state, error occured
	};

	bool has_interrupt() const noexcept { return m_interrupt != Interrupt::CLEAR; }

	enum class Resolution {
		ERROR,
		HI, // 128 x  64 (display ratio 2:1)
		LO, //  64 x  32 (display ratio 2:1)
		TP, //  64 x  64 (display ratio 2:1)
		FP, //  64 x 128 (display ratio 2:1)
		MC, // 256 x 192 (display ratio 4:3)
	};

/*==================================================================*/

	u32 m_target_cpf{};
	u32 m_cycle_count{};
	Interrupt m_interrupt{};

	u32 m_current_pc{};
	u32 m_register_I{};

	u32 m_delay_timer{};

	u32 m_stack_head{};

	static inline
	std::array<u8, 16>
		s_permaregs_V{};

	alignas(sizeof(u8) * 16)
	std::array<u8, 16>
		m_registers_V{};

	alignas(sizeof(u32) * 16)
	std::array<u32, 16>
		m_stack_bank{};

/*==================================================================*/

	void instruction_error(u32 HI, u32 LO) noexcept;
	void trigger_interrupt(Interrupt type) noexcept;
	void trigger_interrupt(Interrupt type, bool cond) noexcept;

private:
	void set_file_permaregs(u32 X) noexcept;
	void get_file_permaregs(u32 X) noexcept;

protected:
	void set_permaregs(u32 X) noexcept;
	void get_permaregs(u32 X) noexcept;

	void copy_game_to_memory(void* dest) noexcept;
	void copy_font_to_memory(void* dest, size_type size) noexcept;

	/*   */ void handle_pre_work_interrupts() noexcept;
	/*   */ void handle_post_work_interrupts() noexcept;

	/*   */ void handle_timer_ticks() noexcept;
	virtual void handle_cycle_loop() noexcept = 0;

	virtual void skip_instruction() noexcept;
	/*   */ void jump_program_to(u32 next) noexcept;

	virtual void push_audio_data() = 0;
	virtual void push_video_data() = 0;

#define LOOP_DISPATCH(LOOP_FUNCTION)					\
	if (has_system_state(EmuState::BENCH)) [[likely]] {	\
		set_frame_stop_flag(has_interrupt());			\
		LOOP_FUNCTION([&]() noexcept { return !get_frame_stop_flag(); }); \
	} else {											\
		LOOP_FUNCTION([&]() noexcept { return !has_interrupt() && m_cycle_count < m_target_cpf; }); \
	}

protected:
	Chip8_CoreInterface(DisplayDevice display_device) noexcept;

public:
	void main_system_loop() override final;

	void append_statistics_data() noexcept override final;

/*==================================================================*/

protected:
	static constexpr auto c_small_font_offset = 0x00;
	static constexpr auto c_large_font_offset = 0x50;

	static constexpr std::array<u8, 240> c_fonts_data = {
		0x60, 0xA0, 0xA0, 0xA0, 0xC0, // 0
		0x40, 0xC0, 0x40, 0x40, 0xE0, // 1
		0xC0, 0x20, 0x40, 0x80, 0xE0, // 2
		0xC0, 0x20, 0x40, 0x20, 0xC0, // 3
		0x20, 0xA0, 0xE0, 0x20, 0x20, // 4
		0xE0, 0x80, 0xC0, 0x20, 0xC0, // 5
		0x40, 0x80, 0xC0, 0xA0, 0x40, // 6
		0xE0, 0x20, 0x60, 0x40, 0x40, // 7
		0x40, 0xA0, 0x40, 0xA0, 0x40, // 8
		0x40, 0xA0, 0x60, 0x20, 0x40, // 9
		0x40, 0xA0, 0xE0, 0xA0, 0xA0, // A
		0xC0, 0xA0, 0xC0, 0xA0, 0xC0, // B
		0x60, 0x80, 0x80, 0x80, 0x60, // C
		0xC0, 0xA0, 0xA0, 0xA0, 0xC0, // D
		0xE0, 0x80, 0xC0, 0x80, 0xE0, // E
		0xE0, 0x80, 0xC0, 0x80, 0x80, // F

		0x7C, 0xC6, 0xCE, 0xDE, 0xD6, 0xF6, 0xE6, 0xC6, 0x7C, 0x00, // 0
		0x10, 0x30, 0xF0, 0x30, 0x30, 0x30, 0x30, 0x30, 0xFC, 0x00, // 1
		0x78, 0xCC, 0xCC, 0x0C, 0x18, 0x30, 0x60, 0xCC, 0xFC, 0x00, // 2
		0x78, 0xCC, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0xCC, 0x78, 0x00, // 3
		0x0C, 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x1E, 0x00, // 4
		0xFC, 0xC0, 0xC0, 0xC0, 0xF8, 0x0C, 0x0C, 0xCC, 0x78, 0x00, // 5
		0x38, 0x60, 0xC0, 0xC0, 0xF8, 0xCC, 0xCC, 0xCC, 0x78, 0x00, // 6
		0xFE, 0xC6, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00, // 7
		0x78, 0xCC, 0xCC, 0xEC, 0x78, 0xDC, 0xCC, 0xCC, 0x78, 0x00, // 8
		0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x18, 0x18, 0x30, 0x70, 0x00, // 9
		/* ------ omit segment below if legacy superchip ------ */
		0x30, 0x78, 0xCC, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0xCC, 0x00, // A
		0xFC, 0x66, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x66, 0xFC, 0x00, // B
		0x3C, 0x66, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x66, 0x3C, 0x00, // C
		0xF8, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00, // D
		0xFE, 0x62, 0x60, 0x64, 0x7C, 0x64, 0x60, 0x62, 0xFE, 0x00, // E
		0xFE, 0x66, 0x62, 0x64, 0x7C, 0x64, 0x60, 0x60, 0xF0, 0x00, // F
	};
	static inline std::array<u8, 240> s_fonts_data = c_fonts_data;

	static constexpr std::array<RGBA, 16> c_bit_colors = { // 0-1 monochrome, 0-15 palette color
		0x181C20FF, 0xE4DCD4FF, 0x8C8884FF, 0x403C38FF,
		0xD82010FF, 0x40D020FF, 0x1040D0FF, 0xE0C818FF,
		0x501010FF, 0x105010FF, 0x50B0C0FF, 0xF08010FF,
		0xE06090FF, 0xE0F090FF, 0xB050F0FF, 0x704020FF,
	};
	static inline std::array<RGBA, 16> s_bit_colors = c_bit_colors;

/*==================================================================*/

	#define PX_1 0x37
	#define PX_2 0x67
	#define PX_3 0xE7
	#define PX_4 0xFF

	// Premul table for pixel trails
	static constexpr std::array<u32, 16> c_bit_weight = {
		PX_4, PX_1, PX_2, PX_2,
		PX_3, PX_3, PX_3, PX_3,
		PX_4, PX_4, PX_4, PX_4,
		PX_4, PX_4, PX_4, PX_4,
	};

	#undef PX_1
	#undef PX_2
	#undef PX_3
	#undef PX_4
};

#endif
