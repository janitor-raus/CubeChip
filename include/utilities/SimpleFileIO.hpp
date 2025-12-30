/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <vector>
#include <fstream>
#include <utility>
#include <cstddef>
#include <filesystem>

#include "Concepts.hpp"
#include "Expected.hpp"

/*==================================================================*/

namespace fs {
	using Path = std::filesystem::path;
	using Time = std::filesystem::file_time_type;

	/* Get last modification date of file at the designated path, if any. */
	[[maybe_unused]]
	inline auto last_write_time(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::last_write_time(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto last_write_time(const Path& filePath, Time time) noexcept {
		std::error_code error;
		std::filesystem::last_write_time(filePath, time, error);
		return ::make_expected(true, std::move(error));
	}

	/* Get size of file at the designated path, if any. */
	[[maybe_unused]]
	inline auto file_size(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::file_size(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/*==================================================================*/

	/* Renames (and possibly replaces) file or folder at the designated paths, if any. */
	[[maybe_unused]]
	inline auto rename(const Path& filePath1, const Path& filePath2) noexcept {
		std::error_code error;
		std::filesystem::rename(filePath1, filePath2, error);
		return ::make_expected(true, std::move(error));
	}

	/* Removes file or empty folder at the designated path, if any. */
	[[maybe_unused]]
	inline auto remove(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::remove(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Removes all files/folders at the designated path, if any. */
	[[maybe_unused]]
	inline auto remove_all(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::remove_all(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto create_directory(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::create_directory(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto create_directory(const Path& filePath1, const Path& filePath2) noexcept {
		std::error_code error;
		auto value = std::filesystem::create_directory(filePath1, filePath2, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Create all required directories up to the designated path. */
	[[maybe_unused]]
	inline auto create_directories(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::create_directories(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Check if the designated path leads to an existing location. */
	[[maybe_unused]]
	inline auto exists(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::exists(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Check if the designated path leads to an existing, regular file. */
	[[maybe_unused]]
	inline auto is_regular_file(const Path& filePath) noexcept {
		std::error_code error;
		auto value = std::filesystem::is_regular_file(filePath, error);
		return ::make_expected(std::move(value), std::move(error));
	}
}

/*==================================================================*/

/**
 * @brief Reads binary data from a file at the given path.
 * @returns Vector of <char> binary data, unless an error_code occurred.
 *
 * @param[in] filePath       :: Path to the file in question.
 * @param[in] dataReadSize   :: Amount of bytes to read. If 0, reads the entire file.
 *                              If non-zero, it will attempt to read the requested amount of bytes, and if the read reaches EOF, will not throw an error.
 * @param[in] dataReadOffset :: Absolute read position offset.
 */
[[maybe_unused]]
inline auto readFileData(
	const fs::Path& filePath, std::size_t dataReadSize = 0,
	std::streamoff dataReadOffset = 0
) noexcept -> Expected<std::vector<char>, std::error_code> {
	try {
		auto fileModStampBegin = fs::last_write_time(filePath);
		if (!fileModStampBegin) { return ::make_unexpected(std::move(fileModStampBegin.error())); }

		std::ifstream inFile(filePath, std::ios::binary | std::ios::in);
		if (!inFile) { return ::make_unexpected(std::make_error_code(std::errc::permission_denied)); }

		inFile.seekg(static_cast<std::streampos>(dataReadOffset));
		if (!inFile) { return ::make_unexpected(std::make_error_code(std::errc::invalid_argument)); }

		std::vector<char> fileData{};

		if (dataReadSize) {
			fileData.resize(dataReadSize);
			inFile.read(fileData.data(), dataReadSize);
		} else {
			try {
				fileData.assign(std::istreambuf_iterator(inFile), {});
				if (!inFile.good()) { throw std::exception{}; }
			} catch (const std::exception&) {
				return ::make_unexpected(std::make_error_code(std::errc::not_enough_memory));
			}
		}

		auto fileModStampEnd = fs::last_write_time(filePath);
		if (!fileModStampEnd) { return ::make_unexpected(std::move(fileModStampEnd.error())); }

		if (fileModStampBegin.value() != fileModStampEnd.value()) {
			return ::make_unexpected(std::make_error_code(std::errc::interrupted));
		} else { return fileData; }
	}
	catch (const std::exception&) {
		return ::make_unexpected(std::make_error_code(std::errc::io_error));
	}
}

/*==================================================================*/

template <typename T>
[[maybe_unused]]
inline auto writeFileData(
	const fs::Path& filePath, const T* fileData, std::size_t dataWriteSize,
	std::streamoff dataWriteOffset = 0
) noexcept -> Expected<bool, std::error_code> {
	try {
		std::ofstream outFile(filePath, std::ios::binary | std::ios::out);
		if (!outFile) { return ::make_unexpected(std::make_error_code(std::errc::permission_denied)); }

		outFile.seekp(static_cast<std::streampos>(dataWriteOffset));
		if (!outFile) { return ::make_unexpected(std::make_error_code(std::errc::invalid_argument)); }

		if (outFile.write(reinterpret_cast<const char*>(fileData), dataWriteSize * sizeof(T)))
			{ return true; } else { throw std::exception{}; }
	}
	catch (const std::exception&) {
		return ::make_unexpected(std::make_error_code(std::errc::io_error));
	}
}

template <IsContiguousContainer T>
[[maybe_unused]]
inline auto writeFileData(
	const fs::Path& filePath, const T& fileData, std::size_t dataWriteSize = 0,
	std::streamoff dataWriteOffset = 0
) noexcept {
	return writeFileData(
		filePath, std::data(fileData), dataWriteSize
		? dataWriteSize : std::size(fileData),
		dataWriteOffset
	);
}

template <typename T, std::size_t N>
[[maybe_unused]]
inline auto writeFileData(
	const fs::Path& filePath, const T(&fileData)[N],
	std::streamoff dataWriteOffset = 0
) noexcept {
	return writeFileData(filePath, fileData, N, dataWriteOffset);
}
