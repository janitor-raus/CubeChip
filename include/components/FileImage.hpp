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

	void replace_context(std::unique_ptr<Context> new_ctx) noexcept;

public:
	~FileImage() noexcept;
	FileImage() noexcept;

	FileImage(std::string file) noexcept;

	FileImage(const FileImage&) noexcept;
	FileImage& operator=(const FileImage&) noexcept;

	FileImage(FileImage&& other) noexcept;
	FileImage& operator=(FileImage&& other) noexcept;

public:
	// Threshold at which we spawn a detached thread to unmap
	static constexpr auto async_threshold = 32ull * 1024 * 1024;

	static auto page_size() noexcept -> std::size_t;

	auto span() const noexcept -> std::span<const char>;
	auto data() const noexcept -> const char*;
	auto size() const noexcept -> std::size_t;
	auto path() const noexcept -> std::string;
	bool load(std::string file) noexcept;

	void clear()       noexcept;
	bool valid() const noexcept;

	operator bool()        const noexcept { return valid(); }
	operator std::string() const noexcept { return path(); }
};
