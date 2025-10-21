/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <fmt/chrono.h>
#include <cstdint>
#include <filesystem>
#include <atomic>

#include "BasicLogger.hpp"
#include <AtomSharedPtr.hpp>
#include "ThreadAffinity.hpp"
#include "Millis.hpp"

/*==================================================================*/

static auto monotonicCount() noexcept {
	static std::atomic<std::uint32_t> counter{};
	return counter.fetch_add(1, std::memory_order::relaxed);
}

/*==================================================================*/

template <typename LogLevelType>
	requires (sizeof(LogLevelType) <= sizeof(int))
LogEntry<LogLevelType>::LogEntry(
	std::uint32_t index,
	LogLevelType  level,
	const void* userdata,
	std::string message
) noexcept
	: hash { std::hash<std::thread::id>{}(std::this_thread::get_id()) }
	, time { Millis::raw() }
	, index{ index }
	, level{ level }
	, userdata{ userdata }
	, message { std::move(message) }
{}

/*==================================================================*/

LoggerInstance::LoggerInstance() noexcept {
	mLogFlusherThread = Thread([this](StopToken token)
		noexcept { LogFlusherThreadEntry(token); });
}

bool LoggerInstance::testFlushSize() const noexcept {
	const auto head{ mLogBuffer.head() };
	return head >= mLastFlushPos && \
		head - mLastFlushPos >= (mLogBuffer.size() / 2);
}

bool LoggerInstance::testFlushTime() const noexcept {
	return Millis::now() - mLastFlushTime >= 10000;
}

void LoggerInstance::LogFlusherThreadEntry(StopToken token) noexcept {
	thread_affinity::set_affinity(0b11ull);
	while (!token.stop_requested()) [[likely]] {
		if (testFlushSize() || testFlushTime())
			[[unlikely]] { flushLogBuffer(); }
		Millis::sleep_for(1);
	}
}

void LoggerInstance::flushLogBuffer() noexcept {
	using namespace std::chrono;
	if (!mLogFile) { return; }

	const auto snapshot{ mLogBuffer.snapshot(0, mLastFlushPos).fast() };
	if (snapshot.size() == 0) { mLastFlushTime = Millis::now(); return; }
	for (const auto& entry : snapshot) {
		mLogFile << fmt::format("{}) {} {:>5} > {}",
			entry.index, NanoTime(entry.time).format(),
			entry.level.to_string(), entry.message) << '\n';
	}

	mLogFile.flush();
	mLastFlushPos += snapshot.size();
	mLastFlushTime = Millis::now();
}

void LoggerInstance::createLog(
	const std::string& filename,
	const std::string& directory
) noexcept {
		if (filename.empty() || directory.empty()) {
			blog.newEntry<BLOG::ERROR>(
				"Log file name/path cannot be blank!");
			return;
		}

		const auto newPath{ std::filesystem::path(directory) / filename };

		mLogFile.open(newPath, std::ios::trunc);
		if (mLogFile) {
			blog.newEntry<BLOG::INFO>(
				"Logging started on {:%Y-%m-%d %H:%M:%S}",
				std::chrono::system_clock::now());
		} else {
			blog.newEntry<BLOG::ERROR>(
				"Unable to create new Log file: \"{}\"",
				newPath.string());
		}
	}

LoggerInstance::~LoggerInstance() noexcept {
	if (mLogFlusherThread.joinable()) {
		mLogFlusherThread.request_stop();
		mLogFlusherThread.join();
	}
	flushLogBuffer();
}

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

BasicLogger::BasicLogger() noexcept {
	static LoggerInstance logger;
	sMainLog = &logger;
}

void BasicLogger::createLog(const std::string& filename, const std::string& directory) noexcept {
	sMainLog->createLog(filename, directory);
}

template <BLOG::LEVEL LOG_LEVEL>
void BasicLogger::newEntry_(std::string&& message) {
	sMainLog->buffer().push(LogEntry(monotonicCount(),
		BLOG(LOG_LEVEL), nullptr, std::move(message)));
}

template void BasicLogger::newEntry_<BLOG::INFO> (std::string&& message);
template void BasicLogger::newEntry_<BLOG::WARN> (std::string&& message);
template void BasicLogger::newEntry_<BLOG::ERROR>(std::string&& message);
template void BasicLogger::newEntry_<BLOG::FATAL>(std::string&& message);
template void BasicLogger::newEntry_<BLOG::DEBUG>(std::string&& message);

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
