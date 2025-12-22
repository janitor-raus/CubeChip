/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "SimpleFileIO.hpp"

/*==================================================================*/

class FileItem {
	fs::Path m_path{};
	bool m_exists{};

public:
	FileItem() noexcept = default;

	template <typename T> requires(std::is_convertible_v<T, fs::Path>)
	FileItem(T&& path) noexcept : m_path(std::forward<T>(path)), m_exists(true) {}

	bool exists() noexcept {
		auto exists = fs::is_regular_file(m_path);
		return m_exists = !(!exists || !exists.value());
	}

	const fs::Path* operator->() const noexcept { return &m_path; }
	const fs::Path& operator* () const noexcept { return  m_path; }

	bool operator==(const FileItem& other) const noexcept { return m_path == other.m_path; }

	operator fs::Path() const noexcept { return m_path; }
	operator bool()     const noexcept { return m_exists; }
};
