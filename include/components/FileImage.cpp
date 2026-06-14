/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "FileImage.hpp"

#include <utility>
#include <thread>

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

void FileImage::replace_context(std::unique_ptr<Context> new_ctx) noexcept {
	if (m_context && m_context->file_mmap.size() >= async_threshold) {
		// sacrificial thread to unmap the source so we don't block
		std::thread([ctx = std::move(m_context)]() mutable {}).detach();
	}
	m_context = std::move(new_ctx);
}

/*==================================================================*/

FileImage::~FileImage() noexcept = default;
FileImage::FileImage() noexcept
	: m_context(nullptr)
{}

FileImage::FileImage(std::string file) noexcept
	: m_context(std::make_unique<Context>(std::move(file)))
{}

FileImage::FileImage(const FileImage& other) noexcept
	: m_context(std::make_unique<Context>(other.m_context->file_path))
{}

FileImage& FileImage::operator=(const FileImage& other) noexcept {
	if (this != &other) {
		load(other.path());
	}
	return *this;
}

FileImage::FileImage(FileImage&& other) noexcept
	: m_context(std::move(other.m_context))
{}

FileImage& FileImage::operator=(FileImage&& other) noexcept {
	if (this != &other) {
		replace_context(std::move(other.m_context));
	}
	return *this;
}

/*==================================================================*/

auto FileImage::page_size() noexcept -> std::size_t {
	return mio::page_size();
}

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
		replace_context(std::make_unique<Context>(std::move(file)));
	}
	return m_context->file_mmap.is_mapped();
}

void FileImage::clear() noexcept {
	replace_context(nullptr);
}

bool FileImage::valid() const noexcept {
	return m_context && m_context->file_mmap.is_mapped();
}
