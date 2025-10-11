/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <utility>

#include <fmt/format.h>

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

class LoggerInstance;

class BasicLogger final {
	BasicLogger() noexcept;

	BasicLogger(const BasicLogger&) = delete;
	BasicLogger& operator=(const BasicLogger&) = delete;

	static inline LoggerInstance* sMainLog{};

public:
	static auto* initialize() noexcept {
		static BasicLogger self;
		return &self;
	}

	void initLogFile(const std::string& filename, const std::string& directory) noexcept;

private:
	template <BLOG LOG_LEVEL>
	void newEntry_(std::string&& message);

public:
	template <BLOG LOG_LEVEL, typename... Args>
	void newEntry(std::string&& message, Args&&... args) {
		if constexpr (sizeof...(Args) != 0) {
			newEntry_<LOG_LEVEL>(fmt::vformat(message, fmt::make_format_args(args...)));
		} else {
			newEntry_<LOG_LEVEL>(std::move(message));
		}
	}

	template <BLOG LOG_LEVEL, typename... Args>
	void newEntry(const std::string& message, Args&&... args) {
		newEntry<LOG_LEVEL>(std::string(message), std::forward<Args>(args)...);
	}
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

extern BasicLogger& blog;
