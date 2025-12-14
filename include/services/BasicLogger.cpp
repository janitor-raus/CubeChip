/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <fmt/ostream.h>
#include <fmt/chrono.h>

#include <cstdint>
#include <filesystem>
#include <atomic>
#include <utility>
#include <fstream>

#include "BasicLogger.hpp"
#include "ThreadAffinity.hpp"
#include "Millis.hpp"
#include "Thread.hpp"

/*==================================================================*/

static auto monotonicCount() noexcept {
	static std::atomic<std::uint32_t> counter = 1;
	return counter.fetch_add(1, std::memory_order::relaxed);
}

static thread_local fmt::basic_memory_buffer<char, 480> sFormatBuffer;

static void standard_string_formatter_for_LogEntry(const LogEntry& entry) noexcept {
	sFormatBuffer.clear();
	fmt::format_to(sFormatBuffer.begin(), "{0}) {1} {3:>{2}} > {4}\n",
		entry.index, NanoTime(entry.time).format(), BLOG::STR_LEN,
		BLOG(entry.level).as_string(), entry.message);
}

/*==================================================================*/

LogEntry::LogEntry(
	std::uint32_t index,
	BLOG::LEVEL   level,
	std::string message
) noexcept
	: hash    (std::hash<std::thread::id>()(std::this_thread::get_id()))
	, time    (Millis::raw())
	, index   (index)
	, level   (level)
	, message (std::move(message))
{}

std::string LogEntry::as_string() const noexcept {
	::standard_string_formatter_for_LogEntry(*this);
	return std::string(sFormatBuffer.data(), sFormatBuffer.size());
}

/*==================================================================*/

class LoggerInstance {
	friend class BasicLogger;

	using LogBuffer = BasicLogger::LogBuffer;

	std::ofstream mLogFile;
	Thread mFlusherThread;

	std::size_t mLastFlushPos{};
	std::size_t mLastFlushTime{};

	LogBuffer mLogBuffer;

	bool testFlushSize() const noexcept {
		const auto head = mLogBuffer.head();
		return head >= mLastFlushPos && \
			head - mLastFlushPos >= (mLogBuffer.size() / 2);
	}

	bool testFlushTime() const noexcept {
		return Millis::now() - mLastFlushTime >= 10000;
	}

	void flushLogBuffer() noexcept{
		if (!mLogFile) { return; }

		const auto snapshot = mLogBuffer.snapshot(0, mLastFlushPos).fast();
		if (snapshot.size() == 0) { mLastFlushTime = Millis::now(); return; }
		for (const auto& entry : snapshot) {
			standard_string_formatter_for_LogEntry(entry);
			mLogFile.write(sFormatBuffer.data(), sFormatBuffer.size());
		}

		mLogFile.flush();
		mLastFlushPos += snapshot.size();
		mLastFlushTime = Millis::now();
	}

	void createLog(const std::string& filename, const std::string& directory) noexcept {
		#ifdef __APPLE__
			blog.newEntry<BLOG::INF>(
				"Logging started on {:%Y-%m-%d %H:%M:%S} ({})",
				std::chrono::system_clock::now(),
				"timezone not corrected on macOS");
		#else
			blog.newEntry<BLOG::INF>(
				"Logging started on {:%Y-%m-%d %H:%M:%S}",
				std::chrono::current_zone()->to_local(
					std::chrono::system_clock::now()));
		#endif

		if (filename.empty() || directory.empty()) {
			blog.newEntry<BLOG::ERR>(
				"Log file name/path cannot be blank!");
			return;
		}

		const auto newPath = std::filesystem::path(directory) / filename;

		mLogFile.open(newPath, std::ios::trunc);
		if (!mLogFile) {
			blog.newEntry<BLOG::ERR>(
				"Unable to create new Log file: \"{}\"",
				newPath.string());
		}
	}

public:
	LoggerInstance() noexcept {
		mFlusherThread = Thread([this](StopToken token) noexcept {
			thread_affinity::set_affinity(0b11ull);

			do {
				if (testFlushSize() || testFlushTime())
					[[unlikely]] { flushLogBuffer(); }
				Millis::sleep_for(1);
			}
			while (!token.stop_requested());
		});
	}

	~LoggerInstance() noexcept {
		if (mFlusherThread.joinable()) {
			mFlusherThread.request_stop();
			mFlusherThread.join();
		}
		flushLogBuffer();
	}

	auto buffer() const noexcept -> const auto& { return mLogBuffer; }
	auto buffer()       noexcept ->       auto& { return mLogBuffer; }
};

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

BasicLogger::BasicLogger() noexcept
	: mMainLog(std::make_unique<LoggerInstance>())
{}

BasicLogger::~BasicLogger() noexcept {}

auto BasicLogger::buffer() const noexcept -> const LogBuffer& {
	return mMainLog->buffer();
}

void BasicLogger::createLog(const std::string& filename, const std::string& directory) noexcept {
	mMainLog->createLog(filename, directory);
}

template <BLOG::LEVEL LOG_LEVEL>
void BasicLogger::newEntry_(std::string&& message) noexcept {
	mMainLog->buffer().push(LogEntry(monotonicCount(), LOG_LEVEL, message));
}

template void BasicLogger::newEntry_<BLOG::DBG>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::INF>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::WRN>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::ERR>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::FTL>(std::string&&) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
