/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		https://github.com/vog/sha1/blob/master/sha1.hpp
*/

#include "SHA1.hpp"
#include <algorithm>

/*==================================================================*/

#if __has_include(<bit>)
#include <bit>
#endif

static constexpr std::uint32_t rotl(std::uint32_t value, std::uint32_t rotation) noexcept {
#if __has_include(<bit>)
	return std::rotl(value, rotation);
#else
	return (value << rotation) | (value >> (32 - rotation));
#endif
}

/*==================================================================*/

static constexpr std::uint32_t block_mix(std::uint32_t* block, std::size_t i) noexcept {
	return rotl(
		block[(i + 0xD) & 0xF] ^
		block[(i + 0x8) & 0xF] ^
		block[(i + 0x2) & 0xF] ^
		block[(i + 0x0) & 0xF], 1);
}

/*------------------------------------------------------------------*/
/*  (R0+R1), R2, R3, R4 are the different operations used in SHA1   */
/*------------------------------------------------------------------*/

static constexpr void R0(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	z += ((w & (x ^ y)) ^ y) + block[i] + 0x5A827999 + rotl(v, 5);
	w  = rotl(w, 30);
}

static constexpr void R1(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += ((w & (x ^ y)) ^ y) + block[i] + 0x5A827999 + rotl(v, 5);
	w  = rotl(w, 30);
}

static constexpr void R2(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += (w ^ x ^ y) + block[i] + 0x6ED9EBA1 + rotl(v, 5);
	w  = rotl(w, 30);
}

static constexpr void R3(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += (((w | x) & y) | (w & x)) + block[i] + 0x8F1BBCDC + rotl(v, 5);
	w  = rotl(w, 30);
}

static constexpr void R4(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += (w ^ x ^ y) + block[i] + 0xCA62C1D6 + rotl(v, 5);
	w  = rotl(w, 30);
}

/*==================================================================*/

constexpr void SHA1::transform(std::uint32_t* block) noexcept {
	// copy digest[] to working vars
	auto a = m_digest[0];
	auto b = m_digest[1];
	auto c = m_digest[2];
	auto d = m_digest[3];
	auto e = m_digest[4];

	// 4 rounds of 20 operations each, loop unrolled
	R0(block, a, b, c, d, e,  0); R0(block, e, a, b, c, d,  1); R0(block, d, e, a, b, c,  2); R0(block, c, d, e, a, b,  3);
	R0(block, b, c, d, e, a,  4); R0(block, a, b, c, d, e,  5); R0(block, e, a, b, c, d,  6); R0(block, d, e, a, b, c,  7);
	R0(block, c, d, e, a, b,  8); R0(block, b, c, d, e, a,  9); R0(block, a, b, c, d, e, 10); R0(block, e, a, b, c, d, 11);
	R0(block, d, e, a, b, c, 12); R0(block, c, d, e, a, b, 13); R0(block, b, c, d, e, a, 14); R0(block, a, b, c, d, e, 15);
	R1(block, e, a, b, c, d,  0); R1(block, d, e, a, b, c,  1); R1(block, c, d, e, a, b,  2); R1(block, b, c, d, e, a,  3);
	R2(block, a, b, c, d, e,  4); R2(block, e, a, b, c, d,  5); R2(block, d, e, a, b, c,  6); R2(block, c, d, e, a, b,  7);
	R2(block, b, c, d, e, a,  8); R2(block, a, b, c, d, e,  9); R2(block, e, a, b, c, d, 10); R2(block, d, e, a, b, c, 11);
	R2(block, c, d, e, a, b, 12); R2(block, b, c, d, e, a, 13); R2(block, a, b, c, d, e, 14); R2(block, e, a, b, c, d, 15);
	R2(block, d, e, a, b, c,  0); R2(block, c, d, e, a, b,  1); R2(block, b, c, d, e, a,  2); R2(block, a, b, c, d, e,  3);
	R2(block, e, a, b, c, d,  4); R2(block, d, e, a, b, c,  5); R2(block, c, d, e, a, b,  6); R2(block, b, c, d, e, a,  7);
	R3(block, a, b, c, d, e,  8); R3(block, e, a, b, c, d,  9); R3(block, d, e, a, b, c, 10); R3(block, c, d, e, a, b, 11);
	R3(block, b, c, d, e, a, 12); R3(block, a, b, c, d, e, 13); R3(block, e, a, b, c, d, 14); R3(block, d, e, a, b, c, 15);
	R3(block, c, d, e, a, b,  0); R3(block, b, c, d, e, a,  1); R3(block, a, b, c, d, e,  2); R3(block, e, a, b, c, d,  3);
	R3(block, d, e, a, b, c,  4); R3(block, c, d, e, a, b,  5); R3(block, b, c, d, e, a,  6); R3(block, a, b, c, d, e,  7);
	R3(block, e, a, b, c, d,  8); R3(block, d, e, a, b, c,  9); R3(block, c, d, e, a, b, 10); R3(block, b, c, d, e, a, 11);
	R4(block, a, b, c, d, e, 12); R4(block, e, a, b, c, d, 13); R4(block, d, e, a, b, c, 14); R4(block, c, d, e, a, b, 15);
	R4(block, b, c, d, e, a,  0); R4(block, a, b, c, d, e,  1); R4(block, e, a, b, c, d,  2); R4(block, d, e, a, b, c,  3);
	R4(block, c, d, e, a, b,  4); R4(block, b, c, d, e, a,  5); R4(block, a, b, c, d, e,  6); R4(block, e, a, b, c, d,  7);
	R4(block, d, e, a, b, c,  8); R4(block, c, d, e, a, b,  9); R4(block, b, c, d, e, a, 10); R4(block, a, b, c, d, e, 11);
	R4(block, e, a, b, c, d, 12); R4(block, d, e, a, b, c, 13); R4(block, c, d, e, a, b, 14); R4(block, b, c, d, e, a, 15);

	// add the working vars back into digest[]
	m_digest[0] += a;
	m_digest[1] += b;
	m_digest[2] += c;
	m_digest[3] += d;
	m_digest[4] += e;

	// count the number of transformations
	++m_transforms;
}

constexpr void SHA1::bytes_to_block(std::uint32_t* block, const char* src) noexcept {
	// read bytes as byteswapped ints
	for (auto i = 0u; i < c_block_size; ++i) {
		block[i] = (src[4 * i + 3] & 0xFF)
				 | (src[4 * i + 2] & 0xFF) <<  8
				 | (src[4 * i + 1] & 0xFF) << 16
				 | (src[4 * i + 0] & 0xFF) << 24;
	}
}

void SHA1::reset() noexcept {
	m_digest[0] = 0x67452301u;
	m_digest[1] = 0xEFCDAB89u;
	m_digest[2] = 0x98BADCFEu;
	m_digest[3] = 0x10325476u;
	m_digest[4] = 0xC3D2E1F0u;

	std::fill_n(m_buffer, c_buffer_size, '\x00');
	m_transforms = m_tail_size = 0;
}

void SHA1::update(const char* data_pointer, std::size_t byte_count) noexcept {
	// if the buffer is partially filled, top it up first
	if (m_tail_size > 0) {
		const auto chunk_size = std::uint32_t(std::min(
			c_block_bytes - m_tail_size, byte_count));

		std::copy(data_pointer, data_pointer + chunk_size,
			m_buffer + m_tail_size);

		m_tail_size  += chunk_size;
		data_pointer += chunk_size;
		byte_count   -= chunk_size;

		if (m_tail_size == c_block_bytes) {
			std::uint32_t block_array[c_block_size];
			bytes_to_block(block_array, m_buffer);
			transform(block_array);
			m_tail_size = 0;
		}
	}

	// process full blocks directly from the input — no copy
	while (byte_count >= c_block_bytes) {
		std::uint32_t block_array[c_block_size];
		bytes_to_block(block_array, data_pointer);
		transform(block_array);
		data_pointer += c_block_bytes;
		byte_count   -= c_block_bytes;
	}

	// stash any remaining partial block in the buffer
	if (byte_count > 0) {
		std::copy(data_pointer, data_pointer + byte_count, m_buffer);
		m_tail_size = std::uint32_t(byte_count);
	}
}

std::string SHA1::final() noexcept {
	const auto total_hashed_bits =
		8 * (m_transforms * c_block_bytes + m_tail_size);

	// insert message end bit, then pad to end of block with zeroes
	m_buffer[m_tail_size++] = '\x80';

	std::fill(m_buffer + m_tail_size,
		m_buffer + c_buffer_size, '\x00');

	// process the final block
	std::uint32_t block_array[c_block_size];
	bytes_to_block(block_array, m_buffer);

	if (m_tail_size > c_block_bytes - 8) {
		transform(block_array);
		for (auto i = 0u; i < c_block_size - 2; ++i) {
			block_array[i] = 0;
		}
	}

	// append total_hashed_bits, split this u64 into two u32
	block_array[c_block_size - 1] = std::uint32_t(total_hashed_bits >>  0);
	block_array[c_block_size - 2] = std::uint32_t(total_hashed_bits >> 32);
	transform(block_array);

	// convert digest[] to a string
	std::string result(40, '\x00');
	constexpr auto c_hex = "0123456789abcdef";
	for (auto i = 0u; i < c_digest_size; ++i) {
		for (auto j = 0u; j < 8; ++j) {
			result[i * 8 + j] = c_hex[(m_digest[i] >> (28 - j * 4)) & 0xF];
		}
	}

	reset();
	return result;
}
