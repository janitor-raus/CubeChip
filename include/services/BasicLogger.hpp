/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <fmt/format.h>
#include <string>
#include <cstddef>

/*==================================================================*/

enum class BLOG {
	INFO,  // Events that are innocuous and informational.
	WARN,  // Events that are unexpected and warrant attention.
	ERROR, // Events that resulted in a predictable/recoverable error.
	FATAL, // Events that resulted in unrecoverable failure.
	DEBUG, // Events meant for debugging purposes.
};

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

class BasicLogger final {
	BasicLogger() noexcept = default;
	BasicLogger(const BasicLogger&) = delete;
	BasicLogger& operator=(const BasicLogger&) = delete;

public:
	static auto* initialize() noexcept {
		static BasicLogger self;
		return &self;
	}

	bool initLogFile(const std::string& filename, const std::string& directory) noexcept;

private:
	template <BLOG LOG_SEVERITY>
	void writeEntry(const std::string& message);

public:
	template <BLOG LOG_SEVERITY, typename... Args>
	void newEntry(const std::string& message, Args&&... args) {
		if constexpr (sizeof...(Args) == 0) {
			writeEntry<LOG_SEVERITY>(message);
		} else {
			writeEntry<LOG_SEVERITY>(fmt::vformat(message, fmt::make_format_args(args...)));
		}
	}

	// here we have a method whose job will be in the cpp to flush N messages from the static TU ringbuffer to the log file.
	void flushToDisk(std::size_t count = 0) noexcept;
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

extern BasicLogger& blog;
