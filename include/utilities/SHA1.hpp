/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		1) https://github.com/vog/sha1/blob/master/sha1.hpp
		2) https://github.com/noloader/SHA-Intrinsics/blob/master/sha1-x86.c
		3) https://github.com/noloader/SHA-Intrinsics/blob/master/sha1-arm.c
*/

#pragma once

#include <type_traits>
#include <cstdint>
#include <string>
#include <span>

/*==================================================================*/

class SHA1 {
	static constexpr auto c_buffer_size = 64u;
	static constexpr auto c_digest_size =  5u;

	std::uint8_t  m_buffer[c_buffer_size]{};
	std::uint32_t m_digest[c_digest_size]{};

	std::uint32_t m_tail_size  = 0;
	std::uint64_t m_transforms = 0;

	void transform(const std::uint8_t* src, std::size_t byte_count) noexcept;
	void transform_scalar(const std::uint8_t* src, std::size_t byte_count) noexcept;
	void transform_x86(const std::uint8_t* src, std::size_t byte_count) noexcept;
	void transform_arm(const std::uint8_t* src, std::size_t byte_count) noexcept;

public:
	static constexpr auto c_block_size  = 16u; // number of 32-bit integers per SHA1 block
	static constexpr auto c_block_bytes = c_block_size * sizeof(std::uint32_t);

public:
	SHA1() noexcept { reset(); }

	static bool has_hardware_support() noexcept;

	void reset() noexcept;
	void update(const char* src, std::size_t byte_count) noexcept;

	std::string final() noexcept;

public:
	static std::string from(const char* data, std::size_t size) noexcept {
		SHA1 checksum;
		checksum.update(data, size);
		return checksum.final();
	}

	static std::string from(std::span<const char> file_span) noexcept {
		return from(file_span.data(), file_span.size());
	}
};