/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../Chip8_CoreInterface.hpp"

#define ENABLE_CHIP8_MODERN
#if defined(ENABLE_CHIP8_SYSTEM) && defined(ENABLE_CHIP8_MODERN)

/*==================================================================*/

class CHIP8_MODERN final : public Chip8_CoreInterface {
	static constexpr u64 c_sys_memory_size  = 4_KiB;
	static constexpr u32 c_game_load_pos    =   512;
	static constexpr u32 c_sys_boot_pos     =   512;
	static constexpr f32 c_sys_refresh_rate = 60.0f;

	static constexpr u32 c_sys_screen_W = 64;
	static constexpr u32 c_sys_screen_H = 32;

	static constexpr u32 c_sys_speed_hi = 30;
	static constexpr u32 c_sys_speed_lo = 11;

private:
	MemoryBank<c_sys_memory_size>
		m_memory_bank{};

	u8 m_display_buffer[c_sys_screen_W * c_sys_screen_H]{};

	Map2D<u8> m_display_map;

public:
	CHIP8_MODERN() noexcept
		: Chip8_CoreInterface(DisplayDevice(c_sys_screen_W, c_sys_screen_H, "CHIP-8 Modern"))
		, m_display_map(m_display_buffer, c_sys_screen_W, c_sys_screen_H)
	{}

	static constexpr bool validate_program(
		const char* fileData,
		const size_type fileSize
	) noexcept {
		if (!fileData || !fileSize) { return false; }
		return fileSize + c_game_load_pos <= c_sys_memory_size;
	}

private:
	void initialize_system() noexcept override;
	void handle_cycle_loop() noexcept override;

	template <typename Lambda>
	void instruction_loop(Lambda&& condition) noexcept;

	void push_audio_data() noexcept override;
	void push_video_data() noexcept override;

/*==================================================================*/
	#pragma region 0 instruction branch

	// 00E0 - erase whole display
	void instruction_00E0() noexcept;
	// 00EE - return from subroutine
	void instruction_00EE() noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 1 instruction branch

	// 1NNN - jump to NNN
	void instruction_1NNN(u32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 2 instruction branch

	// 2NNN - call subroutine at NNN
	void instruction_2NNN(u32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 3 instruction branch

	// 3XNN - skip next instruction if VX == NN
	void instruction_3xNN(u32 X, u32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 4 instruction branch

	// 4XNN - skip next instruction if VX != NN
	void instruction_4xNN(u32 X, u32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 5 instruction branch

	// 5XY0 - skip next instruction if VX == VY
	void instruction_5xy0(u32 X, u32 Y) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 6 instruction branch

	// 6XNN - set VX = NN
	void instruction_6xNN(u32 X, u32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 7 instruction branch

	// 7XNN - set VX = VX + NN
	void instruction_7xNN(u32 X, u32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 8 instruction branch

	// 8XY0 - set VX = VY
	void instruction_8xy0(u32 X, u32 Y) noexcept;
	// 8XY1 - set VX = VX | VY
	void instruction_8xy1(u32 X, u32 Y) noexcept;
	// 8XY2 - set VX = VX & VY
	void instruction_8xy2(u32 X, u32 Y) noexcept;
	// 8XY3 - set VX = VX ^ VY
	void instruction_8xy3(u32 X, u32 Y) noexcept;
	// 8XY4 - set VX = VX + VY, VF = carry
	void instruction_8xy4(u32 X, u32 Y) noexcept;
	// 8XY5 - set VX = VX - VY, VF = !borrow
	void instruction_8xy5(u32 X, u32 Y) noexcept;
	// 8XY7 - set VX = VY - VX, VF = !borrow
	void instruction_8xy7(u32 X, u32 Y) noexcept;
	// 8XY6 - set VX = VY >> 1, VF = carry
	void instruction_8xy6(u32 X, u32 Y) noexcept;
	// 8XYE - set VX = VY << 1, VF = carry
	void instruction_8xyE(u32 X, u32 Y) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region 9 instruction branch

	// 9XY0 - skip next instruction if VX != VY
	void instruction_9xy0(u32 X, u32 Y) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region A instruction branch

	// ANNN - set I = NNN
	void instruction_ANNN(u32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region B instruction branch

	// BNNN - jump to NNN + V0
	void instruction_BNNN(u32 NNN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region C instruction branch

	// CXNN - set VX = rnd(256) & NN
	void instruction_CxNN(u32 X, u32 NN) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region D instruction branch

	void draw_byte(u32 X, u32 Y, u32 DATA) noexcept;

	// DXYN - draw N sprite rows at VX and VY
	void instruction_DxyN(u32 X, u32 Y, u32 N) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region E instruction branch

	// EX9E - skip next instruction if key VX down (p1)
	void instruction_Ex9E(u32 X) noexcept;
	// EXA1 - skip next instruction if key VX up (p1)
	void instruction_ExA1(u32 X) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region F instruction branch

	// FX07 - set VX = delay timer
	void instruction_Fx07(u32 X) noexcept;
	// FX0A - set VX = key, wait for keypress
	void instruction_Fx0A(u32 X) noexcept;
	// FX15 - set delay timer = VX
	void instruction_Fx15(u32 X) noexcept;
	// FX18 - set sound timer = VX
	void instruction_Fx18(u32 X) noexcept;
	// FX1E - set I = I + VX
	void instruction_Fx1E(u32 X) noexcept;
	// FX29 - set I to 5-byte hex sprite from VX
	void instruction_Fx29(u32 X) noexcept;
	// FX33 - store BCD of VX to RAM at I..I+2
	void instruction_Fx33(u32 X) noexcept;
	// FN55 - store V0..VN to RAM at I..I+N
	void instruction_FN55(u32 N) noexcept;
	// FN65 - load V0..VN from RAM at I..I+N
	void instruction_FN65(u32 N) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
};

#endif
