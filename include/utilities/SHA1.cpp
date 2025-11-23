/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		https://github.com/vog/sha1/blob/master/sha1.hpp
*/

#include "SHA1.hpp"

#include <bit>
#include <fstream>
#include <sstream>
#include <iomanip>

/*==================================================================*/

static constexpr std::size_t BLOCK_INTS  = 16; // number of 32-bit integers per SHA1 block
static constexpr std::size_t BLOCK_BYTES = BLOCK_INTS * 4;

inline static std::uint32_t blk(std::uint32_t* block, std::size_t i) {
	return std::rotl(
		block[(i + 0xD) & 0xF] ^
		block[(i + 0x8) & 0xF] ^
		block[(i + 0x2) & 0xF] ^
		block[(i + 0x0) & 0xF], 1);
}

/*------------------------------------------------------------------*/
/*  (R0+R1), R2, R3, R4 are the different operations used in SHA1   */
/*------------------------------------------------------------------*/

inline static void R0(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) {
	z += ((w & (x ^ y)) ^ y) + block[i] + 0x5A827999 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

inline static void R1(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) {
	block[i] = blk(block, i);
	z += ((w & (x ^ y)) ^ y) + block[i] + 0x5A827999 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

inline static void R2(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) {
	block[i] = blk(block, i);
	z += (w ^ x ^ y) + block[i] + 0x6ED9EBA1 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

inline static void R3(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) {
	block[i] = blk(block, i);
	z += (((w | x) & y) | (w & x)) + block[i] + 0x8F1BBCDC + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

inline static void R4(
	std::uint32_t* block,
	std::uint32_t v, std::uint32_t& w, std::uint32_t x,
	std::uint32_t y, std::uint32_t& z, std::size_t i
) {
	block[i] = blk(block, i);
	z += (w ^ x ^ y) + block[i] + 0xCA62C1D6 + std::rotl(v, 5);
	w  = std::rotl(w, 30);
}

/*------------------------------------------------------------------*/
/*  SHA1 class member functions										*/
/*------------------------------------------------------------------*/

void SHA1::transform(std::uint32_t* block) {
	// copy digest[] to working vars
	auto a = digest[0];
	auto b = digest[1];
	auto c = digest[2];
	auto d = digest[3];
	auto e = digest[4];

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
	digest[0] += a;
	digest[1] += b;
	digest[2] += c;
	digest[3] += d;
	digest[4] += e;

	// count the number of transformations
	++transforms;
}

// Hash a single 512-bit block
void SHA1::buffer_to_block(std::uint32_t* block) {

	// convert the string (byte buffer) to a std::uint32_t array (MSB)
	for (std::size_t i = 0; i < BLOCK_INTS; ++i) {
		block[i] = (buffer[4 * i + 3] & 0xFF)
				 | (buffer[4 * i + 2] & 0xFF) <<  8
				 | (buffer[4 * i + 1] & 0xFF) << 16
				 | (buffer[4 * i + 0] & 0xFF) << 24;
	}
}

void SHA1::reset() noexcept {
	digest[0] = 0x67452301;
	digest[1] = 0xEFCDAB89;
	digest[2] = 0x98BADCFE;
	digest[3] = 0x10325476;
	digest[4] = 0xC3D2E1F0;

	buffer.clear();
	transforms = 0;
}

void SHA1::update(const std::string& s) {
	std::istringstream is(s);
	update(is);
}

void SHA1::update(std::istream& is) {
	while (true) {
		char sbuf[BLOCK_BYTES]{};
		const auto chunksize = BLOCK_BYTES - buffer.size();
		is.read(sbuf, static_cast<std::streamsize>(chunksize));

		buffer.append(sbuf, static_cast<std::size_t>(is.gcount()));
		if (buffer.size() != BLOCK_BYTES) { return; }

		std::uint32_t block[BLOCK_INTS]{};
		buffer_to_block(block);
		transform(block);
		buffer.clear();
	}
}

void SHA1::update(const char* data, std::size_t size) {
	std::size_t offset{};

	while (offset < size) {
		const auto chunksize = std::min(BLOCK_BYTES - buffer.size(), size - offset);

		buffer.append(data + offset, chunksize);
		offset += chunksize;

		if (buffer.size() == BLOCK_BYTES) {
			std::uint32_t block[BLOCK_INTS]{};
			buffer_to_block(block);
			transform(block);
			buffer.clear();
		}
	}
}

std::string SHA1::final() {
	// total number of hashed bits
	const auto total_bits = (transforms * BLOCK_BYTES + buffer.size()) * 8;

	// add padding
	buffer += static_cast<char>(0x80);
	const std::size_t orig_size = buffer.size();
	while (buffer.size() < BLOCK_BYTES)
		{ buffer += static_cast<char>(0x00); }

	std::uint32_t block[BLOCK_INTS]{};
	buffer_to_block(block);

	if (orig_size > BLOCK_BYTES - 8) {
		transform(block);
		for (std::size_t i = 0; i < BLOCK_INTS - 2; ++i)
			{ block[i] = 0; }
	}

	// append total_bits, split this u64 into two std::uint32_t
	block[BLOCK_INTS - 1] = total_bits       & 0xFFFFFFFF;
	block[BLOCK_INTS - 2] = total_bits >> 32 & 0xFFFFFFFF;
	transform(block);

	std::ostringstream result;
	for (auto chunk : digest) {
		result << std::hex << std::setfill('0')
			   << std::setw(8) << chunk;
	}

	reset();

	return result.str();
}

std::string SHA1::from_file(const std::filesystem::path& filePath) {
	std::ifstream stream(filePath, std::ios::binary);
	SHA1 checksum;
	checksum.update(stream);
	return checksum.final();
}

std::string SHA1::from_data(const char* data, std::size_t size) {
	SHA1 checksum;
	checksum.update(data, size);
	return checksum.final();
}
