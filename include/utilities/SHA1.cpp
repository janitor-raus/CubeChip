/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		1) https://github.com/vog/sha1/blob/master/sha1.hpp
		2) https://github.com/noloader/SHA-Intrinsics/blob/master/sha1-x86.c
		3) https://github.com/noloader/SHA-Intrinsics/blob/master/sha1-arm.c
*/

#include "SHA1.hpp"
#include <cstring>
#include <bit>

/*==================================================================*/

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#  define SHA1_X86_INTRINSICS
#endif

#ifdef SHA1_X86_INTRINSICS
#  ifdef _MSC_VER
#    include <intrin.h>
#  endif
#  include <immintrin.h>
#endif

/*==================================================================*/

#if defined(__aarch64__) || defined(_M_ARM64)
#  define SHA1_ARM_INTRINSICS
#endif

#ifdef SHA1_ARM_INTRINSICS
#  include <arm_neon.h>
	// GCC (non-Apple Clang) may house the SHA crypto intrinsics in arm_acle.h
#  if defined(__GNUC__) && !defined(__apple_build_version__)
#    ifdef __ARM_ACLE
#      include <arm_acle.h>
#    endif
#  endif
#  if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#      define NOMINMAX
#    endif
#    include <windows.h>
#  elif !defined(__APPLE__)
#    include <sys/auxv.h>
#    include <asm/hwcap.h>
#  endif
#endif

/*==================================================================*/

#if !defined(SHA1_X86_INTRINSICS) && !defined(SHA1_ARM_INTRINSICS)
#  ifdef _MSC_VER
#    pragma message("SHA1: no hardware acceleration path compiled in — scalar only, has_hardware_support() will always return false")
#  else
#    warning "SHA1: no hardware acceleration path compiled in — scalar only, has_hardware_support() will always return false"
#  endif
#endif

/*==================================================================*/

#ifdef SHA1_X86_INTRINSICS
static bool sha1_x86_supported() noexcept {
	static const bool result = []() noexcept {
#  ifdef _MSC_VER
		int info[4]{};
		__cpuid(info, 0);
		if (info[0] < 7) {
			return false;
		} else {
			__cpuidex(info, 7, 0);
			return bool((info[1] >> 29) & 1);
		}
#  else
		return __builtin_cpu_supports("sha");
#  endif
	}();
	return result;
}
#endif

#ifdef SHA1_ARM_INTRINSICS
static bool sha1_arm_supported() noexcept {
	static const bool result = []() noexcept {
#  if defined(__APPLE__)
		return true;
#  elif defined(_WIN32)
		return bool(IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE));
#  else
		return bool(getauxval(AT_HWCAP) & HWCAP_SHA1);
#  endif
	}();
	return result;
}
#endif

bool SHA1::has_hardware_support() noexcept {
#ifdef SHA1_X86_INTRINSICS
	if (sha1_x86_supported()) { return true; }
#endif
#ifdef SHA1_ARM_INTRINSICS
	if (sha1_arm_supported()) { return true; }
#endif
	return false;
}

/*==================================================================*/

static std::uint32_t block_mix(std::uint32_t* block, std::size_t i) noexcept {
	return std::rotl(
		block[(i + 0xD) & 0xF] ^
		block[(i + 0x8) & 0xF] ^
		block[(i + 0x2) & 0xF] ^
		block[(i + 0x0) & 0xF], 1);
}

/*------------------------------------------------------------------*/
/*  (R0+R1), R2, R3, R4 are the different operations used in SHA1   */
/*------------------------------------------------------------------*/

static void R0(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	z += ((w & (x ^ y)) ^ y) + block[i] + 0x5A827999 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

static void R1(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += ((w & (x ^ y)) ^ y) + block[i] + 0x5A827999 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

static void R2(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += (w ^ x ^ y) + block[i] + 0x6ED9EBA1 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

static void R3(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += (((w | x) & y) | (w & x)) + block[i] + 0x8F1BBCDC + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

static void R4(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) noexcept {
	block[i] = block_mix(block, i);
	z += (w ^ x ^ y) + block[i] + 0xCA62C1D6 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
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

#ifdef SHA1_X86_INTRINSICS
#  if defined(__GNUC__)
[[gnu::target("sha,ssse3,sse4.1")]]
#  endif
void SHA1::transform_x86(const std::uint8_t* src) noexcept {
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

#ifdef SHA1_ARM_INTRINSICS
#  if defined(__GNUC__)
[[gnu::target("arch=armv8-a+crypto")]]
#  endif
void SHA1::transform_arm(const std::uint8_t* src) noexcept {
	uint32x4_t abcd, abcd_save;
	uint32x4_t tmp0, tmp1;
	uint32x4_t msg0, msg1, msg2, msg3;
	std::uint32_t e0, e0_save, e1;

	// Load state
	abcd = vld1q_u32(m_digest);
	e0   = m_digest[4];

	abcd_save = abcd;
	e0_save   = e0;

	// Load message; vrev32q_u8 byte-swaps within each 32-bit lane (big-endian → little-endian).
	// Note: per-word reversal only — word order is NOT reversed, unlike the x86 path.
	msg0 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(src +  0)));
	msg1 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(src + 16)));
	msg2 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(src + 32)));
	msg3 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(src + 48)));

	tmp0 = vaddq_u32(msg0, vdupq_n_u32(0x5A827999u));
	tmp1 = vaddq_u32(msg1, vdupq_n_u32(0x5A827999u));

	// Rounds 0-3
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1cq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg2, vdupq_n_u32(0x5A827999u));
	msg0 = vsha1su0q_u32(msg0, msg1, msg2);

	// Rounds 4-7
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1cq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg3, vdupq_n_u32(0x5A827999u));
	msg0 = vsha1su1q_u32(msg0, msg3);
	msg1 = vsha1su0q_u32(msg1, msg2, msg3);

	// Rounds 8-11
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1cq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg0, vdupq_n_u32(0x5A827999u));
	msg1 = vsha1su1q_u32(msg1, msg0);
	msg2 = vsha1su0q_u32(msg2, msg3, msg0);

	// Rounds 12-15
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1cq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg1, vdupq_n_u32(0x6ED9EBA1u));
	msg2 = vsha1su1q_u32(msg2, msg1);
	msg3 = vsha1su0q_u32(msg3, msg0, msg1);

	// Rounds 16-19
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1cq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg2, vdupq_n_u32(0x6ED9EBA1u));
	msg3 = vsha1su1q_u32(msg3, msg2);
	msg0 = vsha1su0q_u32(msg0, msg1, msg2);

	// Rounds 20-23
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg3, vdupq_n_u32(0x6ED9EBA1u));
	msg0 = vsha1su1q_u32(msg0, msg3);
	msg1 = vsha1su0q_u32(msg1, msg2, msg3);

	// Rounds 24-27
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg0, vdupq_n_u32(0x6ED9EBA1u));
	msg1 = vsha1su1q_u32(msg1, msg0);
	msg2 = vsha1su0q_u32(msg2, msg3, msg0);

	// Rounds 28-31
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg1, vdupq_n_u32(0x6ED9EBA1u));
	msg2 = vsha1su1q_u32(msg2, msg1);
	msg3 = vsha1su0q_u32(msg3, msg0, msg1);

	// Rounds 32-35
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg2, vdupq_n_u32(0x8F1BBCDCu));
	msg3 = vsha1su1q_u32(msg3, msg2);
	msg0 = vsha1su0q_u32(msg0, msg1, msg2);

	// Rounds 36-39
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg3, vdupq_n_u32(0x8F1BBCDCu));
	msg0 = vsha1su1q_u32(msg0, msg3);
	msg1 = vsha1su0q_u32(msg1, msg2, msg3);

	// Rounds 40-43
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1mq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg0, vdupq_n_u32(0x8F1BBCDCu));
	msg1 = vsha1su1q_u32(msg1, msg0);
	msg2 = vsha1su0q_u32(msg2, msg3, msg0);

	// Rounds 44-47
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1mq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg1, vdupq_n_u32(0x8F1BBCDCu));
	msg2 = vsha1su1q_u32(msg2, msg1);
	msg3 = vsha1su0q_u32(msg3, msg0, msg1);

	// Rounds 48-51
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1mq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg2, vdupq_n_u32(0x8F1BBCDCu));
	msg3 = vsha1su1q_u32(msg3, msg2);
	msg0 = vsha1su0q_u32(msg0, msg1, msg2);

	// Rounds 52-55
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1mq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg3, vdupq_n_u32(0xCA62C1D6u));
	msg0 = vsha1su1q_u32(msg0, msg3);
	msg1 = vsha1su0q_u32(msg1, msg2, msg3);

	// Rounds 56-59
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1mq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg0, vdupq_n_u32(0xCA62C1D6u));
	msg1 = vsha1su1q_u32(msg1, msg0);
	msg2 = vsha1su0q_u32(msg2, msg3, msg0);

	// Rounds 60-63
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg1, vdupq_n_u32(0xCA62C1D6u));
	msg2 = vsha1su1q_u32(msg2, msg1);
	msg3 = vsha1su0q_u32(msg3, msg0, msg1);

	// Rounds 64-67
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e0,  tmp0);
	tmp0 = vaddq_u32(msg2, vdupq_n_u32(0xCA62C1D6u));
	msg3 = vsha1su1q_u32(msg3, msg2);
	msg0 = vsha1su0q_u32(msg0, msg1, msg2);

	// Rounds 68-71
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e1,  tmp1);
	tmp1 = vaddq_u32(msg3, vdupq_n_u32(0xCA62C1D6u));
	msg0 = vsha1su1q_u32(msg0, msg3);

	// Rounds 72-75
	e1   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e0,  tmp0);

	// Rounds 76-79
	e0   = vsha1h_u32(vgetq_lane_u32(abcd, 0));
	abcd = vsha1pq_u32(abcd, e1,  tmp1);

	// Accumulate state
	e0  += e0_save;
	abcd = vaddq_u32(abcd_save, abcd);

	// Store state
	vst1q_u32(m_digest, abcd);
	m_digest[4] = e0;

	++m_transforms;
}
#endif

/*==================================================================*/

void SHA1::transform(const std::uint8_t* src) noexcept {

#ifdef SHA1_X86_INTRINSICS
	if (sha1_x86_supported()) { transform_x86(src); return; }
#endif
#ifdef SHA1_ARM_INTRINSICS
	if (sha1_arm_supported()) { transform_arm(src); return; }
#endif

	std::uint32_t block[c_block_size];
	std::memcpy(block, src, SHA1::c_block_bytes);
	if constexpr (std::endian::native != std::endian::big) {
		for (auto i = 0u; i < SHA1::c_block_size; ++i) {
#ifdef _MSC_VER
			block[i] = _byteswap_ulong(block[i]);
#else
			block[i] = __builtin_bswap32(block[i]);
#endif
		}
	}

	transform_scalar(block);
}

void SHA1::reset() noexcept {
	m_digest[0] = 0x67452301u;
	m_digest[1] = 0xEFCDAB89u;
	m_digest[2] = 0x98BADCFEu;
	m_digest[3] = 0x10325476u;
	m_digest[4] = 0xC3D2E1F0u;

	std::memset(m_buffer, 0, c_buffer_size);
	m_transforms = m_tail_size = 0;
}

void SHA1::update(const char* src, std::size_t byte_count) noexcept {
	auto data_buffer = reinterpret_cast<const std::uint8_t*>(src);

	// if the buffer is partially filled, top it up first
	if (m_tail_size > 0) {
		const auto remaining  = c_block_bytes - m_tail_size;
		const auto chunk_size = std::uint32_t(byte_count < remaining ? byte_count : remaining);

		std::memcpy(m_buffer + m_tail_size, data_buffer, chunk_size);

		m_tail_size += chunk_size;
		data_buffer += chunk_size;
		byte_count  -= chunk_size;

		if (m_tail_size == c_block_bytes) {
			transform(m_buffer);
			m_tail_size = 0;
		}
	}

	// process full blocks directly from the input — no copy
	while (byte_count >= c_block_bytes) {
		transform(data_buffer);
		data_buffer += c_block_bytes;
		byte_count  -= c_block_bytes;
	}

	// stash any remaining partial block in the buffer
	if (byte_count > 0) {
		std::memcpy(m_buffer, data_buffer, byte_count);
		m_tail_size = std::uint32_t(byte_count);
	}
}

std::string SHA1::final() noexcept {
	const auto total_hashed_bits =
		8 * (m_transforms * c_block_bytes + m_tail_size);

	// insert message end bit, then pad to end of block with zeroes
	m_buffer[m_tail_size++] = std::uint8_t{0x80};

	std::memset(m_buffer + m_tail_size, 0, c_buffer_size - m_tail_size);

	if (m_tail_size > c_block_bytes - 8) {
		transform(m_buffer);
		std::memset(m_buffer, 0, c_block_bytes - 8);
	}

	// append total_hashed_bits as a big-endian 64-bit integer at bytes [56..63]
	const auto hi = std::uint32_t(total_hashed_bits >> 32);
	const auto lo = std::uint32_t(total_hashed_bits >>  0);
	m_buffer[56] = std::uint8_t(hi >> 24); m_buffer[57] = std::uint8_t(hi >> 16);
	m_buffer[58] = std::uint8_t(hi >>  8); m_buffer[59] = std::uint8_t(hi >>  0);
	m_buffer[60] = std::uint8_t(lo >> 24); m_buffer[61] = std::uint8_t(lo >> 16);
	m_buffer[62] = std::uint8_t(lo >>  8); m_buffer[63] = std::uint8_t(lo >>  0);
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
