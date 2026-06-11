/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		1) https://github.com/vog/sha1/blob/master/sha1.hpp
		2) https://github.com/noloader/SHA-Intrinsics/blob/master/sha1-x86.c
*/

#pragma once

#include <cstdint>
#include <string>

#if __has_include(<span>)
#include <span>
#endif

#if defined(__SHA__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86)))
	#define SHA1_HW_SUPPORT
#endif

/*==================================================================*/

class SHA1 {
	static constexpr auto c_buffer_size = 64u;
	static constexpr auto c_digest_size =  5u;

	char          m_buffer[c_buffer_size]{};
	std::uint32_t m_digest[c_digest_size]{};

	std::uint32_t m_tail_size  = 0;
	std::uint64_t m_transforms = 0;

	void transform(const char* src) noexcept;
	void transform_scalar(std::uint32_t* block) noexcept;
#ifdef SHA1_HW_SUPPORT
	void transform_ni(const char* src) noexcept;
#endif

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

#if __has_include(<span>)
	static std::string from(std::span<const char> file_span) noexcept {
		return from(file_span.data(), file_span.size());
	}
#endif
};