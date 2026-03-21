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
#include <memory>
#include <filesystem>

#include "Concepts.hpp"
#include "Expected.hpp"
#include "StringJoin.hpp"

/*==================================================================*/

namespace fs {
	using Path = std::filesystem::path;
	using Time = std::filesystem::file_time_type;

	/* Get last modification date of file at the designated path, if any. */
	[[maybe_unused]]
	inline auto last_write_time(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::last_write_time(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto last_write_time(const Path& file_path, Time time) noexcept {
		std::error_code error;
		std::filesystem::last_write_time(file_path, time, error);
		return ::make_expected(true, std::move(error));
	}

	/* Get size of file at the designated path, if any. */
	[[maybe_unused]]
	inline auto file_size(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::file_size(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/*==================================================================*/

	/* Renames (and possibly replaces) file or folder at the designated paths, if any. */
	[[maybe_unused]]
	inline auto rename(const Path& file_path_1, const Path& file_path_2) noexcept {
		std::error_code error;
		std::filesystem::rename(file_path_1, file_path_2, error);
		return ::make_expected(true, std::move(error));
	}

	/* Removes file or empty folder at the designated path, if any. */
	[[maybe_unused]]
	inline auto remove(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::remove(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Removes all files/folders at the designated path, if any. */
	[[maybe_unused]]
	inline auto remove_all(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::remove_all(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto create_directory(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::create_directory(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto create_directory(const Path& file_path_1, const Path& file_path_2) noexcept {
		std::error_code error;
		auto value = std::filesystem::create_directory(file_path_1, file_path_2, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Create all required directories up to the designated path. */
	[[maybe_unused]]
	inline auto create_directories(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::create_directories(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Check if the designated path leads to an existing location. */
	[[maybe_unused]]
	inline auto exists(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::exists(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	/* Check if the designated path leads to an existing, regular file. */
	[[maybe_unused]]
	inline auto is_regular_file(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::is_regular_file(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto is_directory(const Path& file_path) noexcept {
		std::error_code error;
		auto value = std::filesystem::is_directory(file_path, error);
		return ::make_expected(std::move(value), std::move(error));
	}

	[[maybe_unused]]
	inline auto is_writable_directory(const Path& dir) noexcept
		-> Expected<bool, std::error_code>
	{
		std::error_code ec;

		const bool is_dir = std::filesystem::is_directory(dir, ec);
		if (ec) { return ::make_unexpected(std::move(ec)); }
		else if (!is_dir) { return false; }

		static std::atomic<unsigned> s_counter = 0;
		const auto test_file = dir / ::join(".__fs_write_test_", std::to_string(
			s_counter.fetch_add(1, std::memory_order::relaxed)), ".tmp");

		{
			std::ofstream stream(test_file, std::ios::out | std::ios::trunc);
			if (!stream.is_open()) {
				return ::make_unexpected(std::make_error_code(std::errc::permission_denied));
			}
		}

		const bool removed = std::filesystem::remove(test_file, ec);
		if (ec) { return ::make_unexpected(std::move(ec));}
		else { return removed; }
	}

	/**
	 * @brief Iterates through each directory entry in the given directory path,
	 * invoking the provided callable for each entry. Said callable must take care to
	 * read and handle the error code at all times, as iterating itself  may produce errors.
	 * @tparam Fn Non-throwing allable type that accepts a directory_entry reference and
	 * an error_code reference, which returns bool indicating whether to stop iteration (true)
	 * or continue (false).
	 * @param dir The directory path to iterate through.
	 * @param fn The callable to invoke for each directory entry.
	 */
	template <class Fn> requires (std::is_nothrow_invocable_r_v
		<bool, Fn, std::filesystem::directory_entry&, std::error_code&>)
	void for_each_dir_entry(const std::filesystem::path& dir, Fn&& fn) noexcept {
		std::error_code ec;

		std::filesystem::directory_iterator
			files(dir, ec);

		while (files != std::filesystem::directory_iterator()) {
			if (!std::forward<Fn>(fn)(*files, ec)) {
				files.increment(ec);
				continue;
			}
			break;
		}
	}
}

/*==================================================================*/

/**
 * @brief Reads binary data from a file at the given path.
 * @returns Vector of <char> binary data, unless an error_code occurred.
 *
 * @param[in] file_path        :: Path to the file in question.
 * @param[in] data_read_size   :: Amount of bytes to read. If 0, reads the entire file.
 *                              If non-zero, it will attempt to read the requested amount of bytes, and if the read reaches EOF, will not throw an error.
 * @param[in] data_read_offset :: Absolute read position offset.
 */
[[maybe_unused]]
inline auto read_file_data(
	const fs::Path& file_path, std::size_t data_read_size = 0,
	std::streamoff data_read_offset = 0
) noexcept -> Expected<std::vector<char>, std::error_code> {
	try {
		auto file_modified_stamp_begin = fs::last_write_time(file_path);
		if (!file_modified_stamp_begin) { return ::make_unexpected(std::move(file_modified_stamp_begin.error())); }

		std::ifstream input_file(file_path, std::ios::binary | std::ios::in);
		if (!input_file) { return ::make_unexpected(std::make_error_code(std::errc::permission_denied)); }

		input_file.seekg(static_cast<std::streampos>(data_read_offset));
		if (!input_file) { return ::make_unexpected(std::make_error_code(std::errc::invalid_argument)); }

		std::vector<char> file_data{};

		if (data_read_size) {
			file_data.resize(data_read_size);
			input_file.read(file_data.data(), data_read_size);
		} else {
			try {
				file_data.assign(std::istreambuf_iterator(input_file), {});
				if (!input_file.good()) { throw std::exception{}; }
			} catch (const std::exception&) {
				return ::make_unexpected(std::make_error_code(std::errc::not_enough_memory));
			}
		}

		auto file_modified_stamp_end = fs::last_write_time(file_path);
		if (!file_modified_stamp_end) { return ::make_unexpected(std::move(file_modified_stamp_end.error())); }

		if (file_modified_stamp_begin.value() != file_modified_stamp_end.value()) {
			return ::make_unexpected(std::make_error_code(std::errc::interrupted));
		} else { return file_data; }
	}
	catch (const std::exception&) {
		return ::make_unexpected(std::make_error_code(std::errc::io_error));
	}
}

/*==================================================================*/

template <typename T>
[[maybe_unused]]
inline auto write_file_data(
	const fs::Path& file_path, const T* file_data, std::size_t data_write_size,
	std::streamoff data_write_offset = 0
) noexcept -> Expected<bool, std::error_code> {
	try {
		std::ofstream output_file(file_path, std::ios::binary | std::ios::out);
		if (!output_file) { return ::make_unexpected(std::make_error_code(std::errc::permission_denied)); }

		output_file.seekp(static_cast<std::streampos>(data_write_offset));
		if (!output_file) { return ::make_unexpected(std::make_error_code(std::errc::invalid_argument)); }

		if (output_file.write(reinterpret_cast<const char*>(file_data), data_write_size * sizeof(T)))
			{ return true; } else { throw std::exception{}; }
	}
	catch (const std::exception&) {
		return ::make_unexpected(std::make_error_code(std::errc::io_error));
	}
}

template <IsContiguousContainer T>
[[maybe_unused]]
inline auto write_file_data(
	const fs::Path& file_path, const T& file_data, std::size_t data_write_size = 0,
	std::streamoff data_write_offset = 0
) noexcept {
	return write_file_data(
		file_path, std::data(file_data), data_write_size
		? data_write_size : std::size(file_data),
		data_write_offset
	);
}

template <typename T, std::size_t N>
[[maybe_unused]]
inline auto write_file_data(
	const fs::Path& file_path, const T(&file_data)[N],
	std::streamoff data_write_offset = 0
) noexcept {
	return write_file_data(file_path, file_data, N, data_write_offset);
}
