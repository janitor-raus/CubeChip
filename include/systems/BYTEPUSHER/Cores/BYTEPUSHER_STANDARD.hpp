/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "../BytePusher_CoreInterface.hpp"

#define ENABLE_BYTEPUSHER_STANDARD
#if defined(ENABLE_BYTEPUSHER_SYSTEM) && defined(ENABLE_BYTEPUSHER_STANDARD)

#include "SystemDescriptor.hpp"
#include "ArrayOps.hpp"

/*==================================================================*/

class BYTEPUSHER_STANDARD final : public BytePusher_CoreInterface {
	static constexpr u64 c_sys_memory_size  = 16_MiB;
	static constexpr f32 c_sys_refresh_rate = 60.0f;

	static constexpr u32 c_sys_screen_W = 256;
	static constexpr u32 c_sys_screen_H = 256;

	static constexpr std::string_view c_supported_extensions[] = { ".BytePusher", ".bp" };

	static constexpr const char* validate_program(std::span<const char> file) noexcept {
		if (file.empty()) { return "empty file"; }
		return (file.size() <= c_sys_memory_size)
			? nullptr : "file too large";
	}

public:
	static constexpr SystemDescriptor descriptor = {
		0, Family::family_pretty_name, Family::family_name, Family::family_desc,
		"BytePusher", "bytepusher_standard", "BytePusher based on the original spec.",
		c_supported_extensions, validate_program
	};

	const SystemDescriptor& get_descriptor() const noexcept override {
		return descriptor;
	}

/*==================================================================*/

private:
	MirroredMemory<c_sys_memory_size>
		m_memory{};

	template<u32 T> requires (T >= 1 && T <= 3)
	u32 read_data(u32 pos) const noexcept {
		if        constexpr (T == 1) {
			return m_memory[pos + 0];
		} else if constexpr (T == 2) {
			return m_memory[pos + 0] << 8
				 | m_memory[pos + 1];
		} else if constexpr (T == 3) {
			return m_memory[pos + 0] << 16
				 | m_memory[pos + 1] << 8
				 | m_memory[pos + 2];
		}
	}

	void instruction_loop() noexcept override;
	void push_audio_data() noexcept override;
	void push_video_data() noexcept override;

public:
	BYTEPUSHER_STANDARD() noexcept
		: BytePusher_CoreInterface(c_sys_screen_W, c_sys_screen_H)
	{}

private:
	void initialize_system() noexcept override;
};

#endif
