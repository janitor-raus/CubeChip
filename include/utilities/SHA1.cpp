/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		1) https://github.com/vog/sha1/blob/master/sha1.hpp
		2) https://github.com/noloader/SHA-Intrinsics/blob/master/sha1-x86.c
*/

#include "SHA1.hpp"
#include <algorithm>

/*==================================================================*/

#ifdef SHA1_HW_SUPPORT
	#if defined(_MSC_VER)
		#include <intrin.h>   // for __cpuidex
	#endif
	#include <immintrin.h>
#endif

#ifdef SHA1_HW_SUPPORT
static bool sha1_ni_supported() noexcept {
	static const bool result = []() noexcept {
	#if defined(_MSC_VER)
		int info[4]{};
		__cpuid(info, 0);
		if (info[0] < 7) {
			return false;
		} else {
			__cpuidex(info, 7, 0);
			return bool((info[1] >> 29) & 1);
		}
	#else
		return __builtin_cpu_supports("sha");
	#endif
	}();
	return result;
}
#endif

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

constexpr static void bytes_to_block(std::uint32_t* block, const char* src) noexcept {
	// read quartets of bytes as Big Endian
	for (auto i = 0u; i < SHA1::c_block_size; ++i) {
		block[i] = (src[4 * i + 3] & 0xFF)
				 | (src[4 * i + 2] & 0xFF) <<  8
				 | (src[4 * i + 1] & 0xFF) << 16
				 | (src[4 * i + 0] & 0xFF) << 24;
	}
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

void SHA1::transform_scalar(std::uint32_t* block) noexcept {
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

	m_digest[0] += a;
	m_digest[1] += b;
	m_digest[2] += c;
	m_digest[3] += d;
	m_digest[4] += e;

	++m_transforms;
}

/*==================================================================*/

#ifdef SHA1_HW_SUPPORT
void SHA1::transform_ni(const char* src) noexcept {
	__m128i abcd, abcd_save, e0, e0_save, e1;
	__m128i msg0, msg1, msg2, msg3;

	// Load state. m_digest[] is [A,B,C,D] as uint32_t in memory order, so after loading:
	// bits[31:0]=A ... bits[127:96]=D. SHA-NI wants A at bits[127:96], so reverse word order.
	abcd = _mm_shuffle_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(m_digest)), 0x1B);
	e0 = _mm_set_epi32(std::int32_t(m_digest[4]), 0, 0, 0);

	abcd_save = abcd;
	e0_save   = e0;

	// Load message words from raw big-endian bytes. The mask performs a full 16-byte reversal:
	// byteswap within each 32-bit word AND reverse word order, placing W[0] at bits[127:96].
	const __m128i mask = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	msg0 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src +  0)), mask);
	msg1 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 16)), mask);
	msg2 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 32)), mask);
	msg3 = _mm_shuffle_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 48)), mask);

	// Rounds 0-3
	e0   = _mm_add_epi32(e0, msg0);
	e1   = abcd;
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   0);

	// Rounds 4-7
	e1   = _mm_sha1nexte_epu32(e1,   msg1);
	e0   = abcd;
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   0);
	msg0 = _mm_sha1msg1_epu32(msg0, msg1);

	// Rounds 8-11
	e0   = _mm_sha1nexte_epu32(e0,   msg2);
	e1   = abcd;
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   0);
	msg1 = _mm_sha1msg1_epu32(msg1, msg2);
	msg0 = _mm_xor_si128(msg0, msg2);

	// Rounds 12-15
	e1   = _mm_sha1nexte_epu32(e1,   msg3);
	e0   = abcd;
	msg0 = _mm_sha1msg2_epu32(msg0, msg3);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   0);
	msg2 = _mm_sha1msg1_epu32(msg2, msg3);
	msg1 = _mm_xor_si128(msg1, msg3);

	// Rounds 16-19
	e0   = _mm_sha1nexte_epu32(e0,   msg0);
	e1   = abcd;
	msg1 = _mm_sha1msg2_epu32(msg1, msg0);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   0);
	msg3 = _mm_sha1msg1_epu32(msg3, msg0);
	msg2 = _mm_xor_si128(msg2, msg0);

	// Rounds 20-23
	e1   = _mm_sha1nexte_epu32(e1,   msg1);
	e0   = abcd;
	msg2 = _mm_sha1msg2_epu32(msg2, msg1);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   1);
	msg0 = _mm_sha1msg1_epu32(msg0, msg1);
	msg3 = _mm_xor_si128(msg3, msg1);

	// Rounds 24-27
	e0   = _mm_sha1nexte_epu32(e0,   msg2);
	e1   = abcd;
	msg3 = _mm_sha1msg2_epu32(msg3, msg2);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   1);
	msg1 = _mm_sha1msg1_epu32(msg1, msg2);
	msg0 = _mm_xor_si128(msg0, msg2);

	// Rounds 28-31
	e1   = _mm_sha1nexte_epu32(e1,   msg3);
	e0   = abcd;
	msg0 = _mm_sha1msg2_epu32(msg0, msg3);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   1);
	msg2 = _mm_sha1msg1_epu32(msg2, msg3);
	msg1 = _mm_xor_si128(msg1, msg3);

	// Rounds 32-35
	e0   = _mm_sha1nexte_epu32(e0,   msg0);
	e1   = abcd;
	msg1 = _mm_sha1msg2_epu32(msg1, msg0);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   1);
	msg3 = _mm_sha1msg1_epu32(msg3, msg0);
	msg2 = _mm_xor_si128(msg2, msg0);

	// Rounds 36-39
	e1   = _mm_sha1nexte_epu32(e1,   msg1);
	e0   = abcd;
	msg2 = _mm_sha1msg2_epu32(msg2, msg1);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   1);
	msg0 = _mm_sha1msg1_epu32(msg0, msg1);
	msg3 = _mm_xor_si128(msg3, msg1);

	// Rounds 40-43
	e0   = _mm_sha1nexte_epu32(e0,   msg2);
	e1   = abcd;
	msg3 = _mm_sha1msg2_epu32(msg3, msg2);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   2);
	msg1 = _mm_sha1msg1_epu32(msg1, msg2);
	msg0 = _mm_xor_si128(msg0, msg2);

	// Rounds 44-47
	e1   = _mm_sha1nexte_epu32(e1,   msg3);
	e0   = abcd;
	msg0 = _mm_sha1msg2_epu32(msg0, msg3);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   2);
	msg2 = _mm_sha1msg1_epu32(msg2, msg3);
	msg1 = _mm_xor_si128(msg1, msg3);

	// Rounds 48-51
	e0   = _mm_sha1nexte_epu32(e0,   msg0);
	e1   = abcd;
	msg1 = _mm_sha1msg2_epu32(msg1, msg0);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   2);
	msg3 = _mm_sha1msg1_epu32(msg3, msg0);
	msg2 = _mm_xor_si128(msg2, msg0);

	// Rounds 52-55
	e1   = _mm_sha1nexte_epu32(e1,   msg1);
	e0   = abcd;
	msg2 = _mm_sha1msg2_epu32(msg2, msg1);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   2);
	msg0 = _mm_sha1msg1_epu32(msg0, msg1);
	msg3 = _mm_xor_si128(msg3, msg1);

	// Rounds 56-59
	e0   = _mm_sha1nexte_epu32(e0,   msg2);
	e1   = abcd;
	msg3 = _mm_sha1msg2_epu32(msg3, msg2);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   2);
	msg1 = _mm_sha1msg1_epu32(msg1, msg2);
	msg0 = _mm_xor_si128(msg0, msg2);

	// Rounds 60-63
	e1   = _mm_sha1nexte_epu32(e1,   msg3);
	e0   = abcd;
	msg0 = _mm_sha1msg2_epu32(msg0, msg3);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   3);
	msg2 = _mm_sha1msg1_epu32(msg2, msg3);
	msg1 = _mm_xor_si128(msg1, msg3);

	// Rounds 64-67
	e0   = _mm_sha1nexte_epu32(e0,   msg0);
	e1   = abcd;
	msg1 = _mm_sha1msg2_epu32(msg1, msg0);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   3);
	msg3 = _mm_sha1msg1_epu32(msg3, msg0);
	msg2 = _mm_xor_si128(msg2, msg0);

	// Rounds 68-71
	e1   = _mm_sha1nexte_epu32(e1,   msg1);
	e0   = abcd;
	msg2 = _mm_sha1msg2_epu32(msg2, msg1);
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   3);
	msg3 = _mm_xor_si128(msg3, msg1);

	// Rounds 72-75
	e0   = _mm_sha1nexte_epu32(e0,   msg2);
	e1   = abcd;
	msg3 = _mm_sha1msg2_epu32(msg3, msg2);
	abcd = _mm_sha1rnds4_epu32(abcd, e0,   3);

	// Rounds 76-79
	e1   = _mm_sha1nexte_epu32(e1,   msg3);
	e0   = abcd;
	abcd = _mm_sha1rnds4_epu32(abcd, e1,   3);

	// Accumulate state
	e0   = _mm_sha1nexte_epu32(e0, e0_save);
	abcd = _mm_add_epi32(abcd, abcd_save);

	// Store state (reverse word order back to match m_digest[] layout)
	_mm_storeu_si128(reinterpret_cast<__m128i*>(m_digest), _mm_shuffle_epi32(abcd, 0x1B));
	m_digest[4] = std::uint32_t(_mm_extract_epi32(e0, 3));

	++m_transforms;
}
#endif

/*==================================================================*/

void SHA1::transform(const char* src) noexcept {
#ifdef SHA1_HW_SUPPORT
	if (sha1_ni_supported()) {
		transform_ni(src);
		return;
	}
#endif
	std::uint32_t block[c_block_size];
	bytes_to_block(block, src);
	transform_scalar(block);
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
			transform(m_buffer);
			m_tail_size = 0;
		}
	}

	// process full blocks directly from the input — no copy
	while (byte_count >= c_block_bytes) {
		transform(data_pointer);
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

	if (m_tail_size > c_block_bytes - 8) {
		transform(m_buffer);
		std::fill_n(m_buffer, c_block_bytes - 8, '\x00');
	}

	// append total_hashed_bits as a big-endian 64-bit integer at bytes [56..63]
	const auto hi = std::uint32_t(total_hashed_bits >> 32);
	const auto lo = std::uint32_t(total_hashed_bits >>  0);
	m_buffer[56] = char(hi >> 24); m_buffer[57] = char(hi >> 16);
	m_buffer[58] = char(hi >>  8); m_buffer[59] = char(hi >>  0);
	m_buffer[60] = char(lo >> 24); m_buffer[61] = char(lo >> 16);
	m_buffer[62] = char(lo >>  8); m_buffer[63] = char(lo >>  0);
	transform(m_buffer);

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
