/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		https://github.com/vog/sha1/blob/master/sha1.hpp
*/

#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <span>

/*==================================================================*/

class SHA1 {
	std::array<char, 64> m_buffer{};
	std::uint32_t m_digest[5]{};
	std::uint32_t m_buf_offset = 0;
	std::uint64_t m_transforms = 0;

	constexpr void transform(std::uint32_t* block) noexcept;
	constexpr void buffer_to_block(std::uint32_t* block) noexcept;

public:
	static constexpr auto c_block_size  = 16u; // number of 32-bit integers per SHA1 block
	static constexpr auto c_block_bytes = c_block_size * sizeof(std::uint32_t);

public:
	SHA1() noexcept { reset(); }

	void reset() noexcept;
	void update(const char* data, std::size_t size) noexcept;

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