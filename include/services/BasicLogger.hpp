/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <cstdint>
#include <memory>

#include <fmt/format.h>

#include "SlidingRingBuffer.hpp"
#include "HDIS_HCIS.hpp"

/*==================================================================*/

struct BLOG {
	enum class LEVEL {
		DBG, // Events meant for debugging purposes.
		INF, // Events that are innocuous and informational.
		WRN, // Events that are unexpected and warrant attention.
		ERR, // Events that resulted in a predictable/recoverable error.
		FTL, // Events that resulted in unrecoverable failure.
	};

private:
	LEVEL value{};

public:
	static constexpr LEVEL DBG{ LEVEL::DBG };
	static constexpr LEVEL INF{ LEVEL::INF };
	static constexpr LEVEL WRN{ LEVEL::WRN };
	static constexpr LEVEL ERR{ LEVEL::ERR };
	static constexpr LEVEL FTL{ LEVEL::FTL };

	static constexpr auto LENGTH{ 7u }; // hardcoded for now

	constexpr BLOG() noexcept = default;
	constexpr BLOG(LEVEL level) noexcept : value{ level } {}

	// Severity string representations
	constexpr const char* to_string() const noexcept {
		switch (value) {
			case DBG: return "DEBUG";
			case INF: return "INFO";
			case WRN: return "WARNING";
			case ERR: return "ERROR";
			case FTL: return "FATAL";
			default:  return "UNKNOWN";
		}
	}
	// Severity color codes in RGBA format
	constexpr std::uint32_t to_color() const noexcept {
		switch (value) {
			case DBG: return 0x77DDDDFF;
			case INF: return 0x44BB00FF;
			case WRN: return 0xFFCC00FF;
			case ERR: return 0xEE9933FF;
			case FTL: return 0xCC3300FF;
			default:  return 0xFFFFFFFF;
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
	#pragma region BasicLogger Singleton Class

using LogBufferT = SlidingRingBuffer
	<LogEntry<BLOG>, 512>;

class LoggerInstance;

class BasicLogger final {
	BasicLogger() noexcept;
	~BasicLogger() noexcept;

	BasicLogger(const BasicLogger&) = delete;
	BasicLogger& operator=(const BasicLogger&) = delete;

	const std::unique_ptr
		<LoggerInstance> mMainLog{};

public:
	static auto* initialize() noexcept {
		static BasicLogger self;
		return &self;
	}

	const LogBufferT& buffer() const noexcept;
	const LogBufferT* operator->() const noexcept { return &buffer(); }

	void createLog(const std::string& filename, const std::string& directory) noexcept;

private:
	template <BLOG::LEVEL LOG_LEVEL>
	void newEntry_(std::string&& message) noexcept;

public:
	template <BLOG::LEVEL LOG_LEVEL, typename... Args>
	void newEntry(std::string&& message, Args&&... args) noexcept {
		newEntry_<LOG_LEVEL>(fmt::vformat(message, fmt::make_format_args(args...)));
	}

	template <BLOG::LEVEL LOG_LEVEL, typename... Args>
	void newEntry(const std::string& message, Args&&... args) noexcept {
		newEntry_<LOG_LEVEL>(fmt::vformat(message, fmt::make_format_args(args...)));
	}
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

extern BasicLogger& blog;
