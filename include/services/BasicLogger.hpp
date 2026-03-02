/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <memory>

#include <fmt/base.h>

#include "SlidingRingBuffer.hpp"

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
	LEVEL m_value{};

public:
	static constexpr LEVEL DBG = LEVEL::DBG;
	static constexpr LEVEL INF = LEVEL::INF;
	static constexpr LEVEL WRN = LEVEL::WRN;
	static constexpr LEVEL ERR = LEVEL::ERR;
	static constexpr LEVEL FTL = LEVEL::FTL;

	static constexpr auto STR_LEN = 7u; // hardcoded for now

	constexpr BLOG() noexcept = default;
	constexpr BLOG(LEVEL level) noexcept : m_value(level) {}

	// Severity string representations
	constexpr const char* as_string() const noexcept {
		switch (m_value) {
			case DBG: return "DEBUG";
			case INF: return "INFO";
			case WRN: return "WARNING";
			case ERR: return "ERROR";
			case FTL: return "FATAL";
			default:  return "UNKNOWN";
		}
	}
	// Severity color codes in RGBA format
	constexpr unsigned as_color() const noexcept {
		switch (m_value) {
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

struct LogEntry {
	const std::uint32_t thread{}; // thread id, expected to be monotonic
	const std::uint32_t index{};  // entry index, expected to be monotonic
	const std::uint32_t source{}; // component source id, key for filtering
	const BLOG::LEVEL   level{};  // severity level, offers additional methods
	const std::int64_t  time{};   // timestamp, expected to be ns precision
	const std::string message{};  // the actual string message

	constexpr LogEntry() noexcept = default;
	LogEntry(BLOG::LEVEL level, std::uint32_t source, std::string message) noexcept;

	std::string as_string() const noexcept;
};

/*==================================================================*/

[[nodiscard]] std::uint32_t get_source_index(std::string_view src_name) noexcept;
[[nodiscard]] std::string   get_source_name(std::uint32_t src_id) noexcept;

class ScopedLogSource {
	std::uint32_t m_prev_source_id{};

public:
	template <typename... Args>
	ScopedLogSource(fmt::format_string<Args...> fmt, Args&&... args) noexcept
		: ScopedLogSource([&]() {
			try { return fmt::format(fmt, std::forward<Args>(args)...); }
			catch (...) { return ""; }
		}())
	{}

	ScopedLogSource(std::string_view src_name) noexcept;
	~ScopedLogSource() noexcept;
};

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

class BasicLoggerContext;

class BasicLogger final {
	BasicLogger() noexcept;
	~BasicLogger() noexcept;

	BasicLogger(const BasicLogger&) = delete;
	BasicLogger& operator=(const BasicLogger&) = delete;

	std::unique_ptr<BasicLoggerContext> m_context{};

public:
	using LogBuffer = SlidingRingBuffer<LogEntry, 1024>;

public:
	static auto* initialize() noexcept {
		static BasicLogger self;
		return &self;
	}

	void shutdown() noexcept;

	auto buffer() const noexcept -> const LogBuffer*;
	auto operator->() const noexcept -> const LogBuffer* { return buffer(); }

	void create_log(const std::string& filename, const std::string& directory) noexcept;
	auto get_log_path() const noexcept -> std::string;

private:
	void push_entry(BLOG::LEVEL level, std::string&& message) noexcept;

public:
	#if !defined(NDEBUG) || defined(DEBUG) // only push these in debug builds
	template <typename... Args>
	void debug(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
		push_entry(BLOG::DBG, fmt::format(fmt, std::forward<Args>(args)...));
	}
	void debug(std::string&& message) noexcept { push_entry(BLOG::DBG, std::move(message)); }
	void debug(const char* message) noexcept { push_entry(BLOG::DBG, std::string(message)); }
	#else
	template <typename... Args>
	void debug(fmt::format_string<Args...>, Args&&...) noexcept {}
	void debug(std::string&&) noexcept {}
	void debug(const char*) noexcept {}
	#endif

	template <typename... Args>
	void info(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
		push_entry(BLOG::INF, fmt::format(fmt, std::forward<Args>(args)...));
	}
	void info(std::string&& message) noexcept { push_entry(BLOG::INF, std::move(message)); }
	void info(const char* message) noexcept { push_entry(BLOG::INF, std::string(message)); }

	template <typename... Args>
	void warn(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
		push_entry(BLOG::WRN, fmt::format(fmt, std::forward<Args>(args)...));
	}
	void warn(std::string&& message) noexcept { push_entry(BLOG::WRN, std::move(message)); }
	void warn(const char* message) noexcept { push_entry(BLOG::WRN, std::string(message)); }

	template <typename... Args>
	void error(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
		push_entry(BLOG::ERR, fmt::format(fmt, std::forward<Args>(args)...));
	}
	void error(std::string&& message) noexcept { push_entry(BLOG::ERR, std::move(message)); }
	void error(const char* message) noexcept { push_entry(BLOG::ERR, std::string(message)); }

	template <typename... Args>
	void fatal(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
		push_entry(BLOG::FTL, fmt::format(fmt, std::forward<Args>(args)...));
	}
	void fatal(std::string&& message) noexcept { push_entry(BLOG::FTL, std::move(message)); }
	void fatal(const char* message) noexcept { push_entry(BLOG::FTL, std::string(message)); }
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

extern BasicLogger& blog;
