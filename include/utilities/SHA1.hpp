/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Adapted from public domain source code at:
		https://github.com/vog/sha1/blob/master/sha1.hpp
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <span>

/*==================================================================*/

class SHA1 {
	std::string   buffer{};
	std::uint32_t digest[5]{};
	std::uint64_t transforms{};

	void transform(std::uint32_t* block);
	void buffer_to_block(std::uint32_t* block);

public:
	SHA1() noexcept { reset(); }

	void reset() noexcept;

	void update(const std::string& s);
	void update(std::istream& is);
	void update(const char* data, std::size_t size);

	std::string final();

	static std::string from_file(std::string file_path);
	static std::string from_span(std::span<const char> file_span);
	static std::string from_data(const char* data, std::size_t size);
};
