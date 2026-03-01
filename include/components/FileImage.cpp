/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <system_error>
#include <utility>

#include "FileImage.hpp"
#include "mio/mmap.hpp"

/*==================================================================*/

struct FileImage::Context {
	std::string      file_path{};
	mio::mmap_source file_mmap{};

	Context() noexcept = default;
	Context(std::string file) noexcept
		: file_path(std::move(file))
	{
		std::error_code error;
		file_mmap.map(file_path, error);
		if (error) { file_path.clear(); }
	}
};

FileImage::~FileImage() noexcept = default;
FileImage::FileImage() noexcept
	: m_context(std::make_unique<Context>())
{}

FileImage::FileImage(std::string file) noexcept
	: m_context(std::make_unique<Context>(std::move(file)))
{}

FileImage::FileImage(FileImage&& other) noexcept
	: m_context(std::move(other.m_context))
{}

auto FileImage::span() const noexcept -> std::span<const char> {
	return std::span<const char>(data(), size());
}

auto FileImage::data() const noexcept -> const char* {
	return m_context ? m_context->file_mmap.data() : nullptr;
}

auto FileImage::size() const noexcept -> std::size_t {
	return m_context ? m_context->file_mmap.size() : std::size_t();
}

auto FileImage::path() const noexcept -> std::string {
	return m_context ? m_context->file_path : std::string();
}

bool FileImage::load(std::string file) noexcept {
	if (!m_context || m_context->file_path != file) {
		m_context = std::make_unique<Context>(std::move(file));
	}
	return m_context->file_mmap.is_mapped();
}

void FileImage::clear() noexcept {
	m_context = std::make_unique<Context>();
}

bool FileImage::valid() const noexcept {
	return m_context && m_context->file_mmap.is_mapped();
}
