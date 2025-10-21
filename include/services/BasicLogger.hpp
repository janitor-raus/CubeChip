/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <fstream>

#include <fmt/format.h>

#include "SlidingRingBuffer.hpp"
#include "Thread.hpp"

/*==================================================================*/

struct BLOG {
	enum class LEVEL {
		DEBUG, // Events meant for debugging purposes.
		INFO,  // Events that are innocuous and informational.
		WARN,  // Events that are unexpected and warrant attention.
		ERROR, // Events that resulted in a predictable/recoverable error.
		FATAL, // Events that resulted in unrecoverable failure.
	};

private:
	LEVEL value{};

public:
	static constexpr LEVEL DEBUG = LEVEL::DEBUG;
	static constexpr LEVEL INFO  = LEVEL::INFO;
	static constexpr LEVEL WARN  = LEVEL::WARN;
	static constexpr LEVEL ERROR = LEVEL::ERROR;
	static constexpr LEVEL FATAL = LEVEL::FATAL;

	constexpr BLOG() noexcept = default;
	constexpr BLOG(LEVEL level) noexcept : value{ level } {}

	constexpr std::string_view
	to_string() const noexcept {
		switch (value) {
			case INFO:  return "INFO";
			case WARN:  return "WARN";
			case ERROR: return "ERROR";
			case FATAL: return "FATAL";
			case DEBUG: return "DEBUG";
			default: return "UNKN";
		}
	}
};

/*==================================================================*/

template <typename LogLevelType>
	requires (sizeof(LogLevelType) <= sizeof(int))
struct alignas(HDIS) LogEntry {
	std::uint64_t hash{};  // thread hash, to be filled in automatically
	std::int64_t  time{};  // timestamp, expected to be ns
	std::uint32_t index{}; // entry index, expected to be monotonic
	LogLevelType  level{}; // log level, usually tied to an enum
	const void* userdata{}; // pointer to additional data, if any
	std::string message{};  // self-explanatory

	constexpr LogEntry() noexcept = default;
	LogEntry(
		std::uint32_t index,
		LogLevelType  level,
		const void* userdata,
		std::string message
	) noexcept;
};

/*==================================================================*/

class BasicLogger;

class LoggerInstance {
	friend class BasicLogger;

	using LogBufferT = SlidingRingBuffer
		<LogEntry<BLOG>, 512>;

	LogBufferT mLogBuffer;

	alignas(HDIS)
		std::ofstream mLogFile;

	std::size_t mLastFlushPos{};
	std::size_t mLastFlushTime{};
	Thread mLogFlusherThread;

	LoggerInstance() noexcept;

	bool testFlushSize() const noexcept;
	bool testFlushTime() const noexcept;

	void LogFlusherThreadEntry(StopToken token) noexcept;

	void flushLogBuffer() noexcept;

	void createLog(const std::string& filename, const std::string& directory) noexcept;

public:
	~LoggerInstance() noexcept;

	auto& buffer() noexcept { return mLogBuffer; }
};

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

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

	const auto& buffer() const noexcept { return sMainLog->buffer(); }
	const auto* operator->() const noexcept { return &buffer(); }

	void createLog(const std::string& filename, const std::string& directory) noexcept;

private:
	template <BLOG::LEVEL LOG_LEVEL>
	void newEntry_(std::string&& message);

public:
	template <BLOG::LEVEL LOG_LEVEL, typename... Args>
	void newEntry(std::string&& message, Args&&... args) {
		if constexpr (sizeof...(Args) != 0) {
			newEntry_<LOG_LEVEL>(fmt::vformat(message, fmt::make_format_args(args...)));
		} else {
			newEntry_<LOG_LEVEL>(std::move(message));
		}
	}

	template <BLOG::LEVEL LOG_LEVEL, typename... Args>
	void newEntry(const std::string& message, Args&&... args) {
		newEntry<LOG_LEVEL>(std::string(message), std::forward<Args>(args)...);
	}
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

extern BasicLogger& blog;
