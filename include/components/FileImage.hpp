/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>
#include <string>
#include <span>

/*==================================================================*/

class FileImage {
	struct Context;

	std::unique_ptr<Context>
		m_context{};

public:
	~FileImage() noexcept;
	FileImage() noexcept;

	FileImage(const FileImage&) = delete;
	FileImage& operator=(const FileImage&) = delete;

	FileImage(std::string file) noexcept;
	FileImage(FileImage&& other) noexcept;

	auto span() const noexcept -> std::span<const char>;
	auto data() const noexcept -> const char*;
	auto size() const noexcept -> std::size_t;
	auto path() const noexcept -> std::string;
	bool load(std::string file) noexcept;

	void clear()       noexcept;
	bool valid() const noexcept;

	operator bool()        const noexcept { return valid(); }
	operator std::string() const noexcept { return path(); }
	operator std::span<const char>() const noexcept { return span(); }
};
