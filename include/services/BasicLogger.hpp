/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "Typedefs.hpp"
#include "SimpleRingBuffer.hpp"

/*==================================================================*/

enum class BLOG {
	INFO,  // Events that are innocuous and informational.
	WARN,  // Events that are unexpected and warrant attention.
	ERROR, // Events that resulted in a predictable/recoverable error.
	FATAL,  // Events that resulted in unrecoverable failure.
	DEBUG, // Events meant for debugging purposes.
};

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

class BasicLogger final {
	SimpleRingBuffer<Str, 512>
		mLogBuffer;

	Path mLogPath{};

	BasicLogger() noexcept = default;
	BasicLogger(const BasicLogger&) = delete;
	BasicLogger& operator=(const BasicLogger&) = delete;

	StrV getSeverity(BLOG type) const noexcept;

public:
	static auto* initialize() noexcept {
		static BasicLogger self;
		return &self;
	}

	bool initLogFile(const Str& filename, const Path& directory) noexcept;

private:
	void writeEntry(BLOG type, const Str& message);

public:
	template <typename... Args>
	void newEntry(BLOG type, const Str& message, Args&&... args) {
		if constexpr (sizeof...(Args) == 0) {
			writeEntry(type, message);
		} else {
			writeEntry(type, fmt::vformat(message, fmt::make_format_args(args...)));
		}
	}
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

extern BasicLogger& blog;
