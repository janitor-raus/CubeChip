/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <fmt/chrono.h>
#include <utility>
#include <string_view>
#include <filesystem>

#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"
#include "SlidingRingBuffer.hpp"
#include "ThreadAffinity.hpp"
#include "Thread.hpp"
#include "Millis.hpp"

/*==================================================================*/

class LoggerInstance {
	SlidingRingBuffer
		<std::string, 512> mLogBuffer;

	alignas(HDIS)
	std::ofstream mLogFile;

	std::size_t mLastFlushPos{};
	std::size_t mLastFlushTime{};
	Thread mLogFlusherThread;

	friend class BasicLogger;
	LoggerInstance() noexcept {
		mLogFlusherThread = Thread([this](StopToken token)
			noexcept { LogFlusherThreadEntry(token); });
	}

	bool testFlushSize() const noexcept {
		const auto head{ mLogBuffer.head() };
		return head >= mLastFlushPos && \
			head - mLastFlushPos >= (mLogBuffer.size() / 2);
	}

	bool testFlushTime() const noexcept {
		return Millis::now() - mLastFlushTime >= 10000;
	}

	void LogFlusherThreadEntry(StopToken token) noexcept {
		thread_affinity::set_affinity(0b11ull);
		while (!token.stop_requested()) [[likely]] {
			if (testFlushSize() || testFlushTime())
				[[unlikely]] { flushLogBuffer(); }
			Millis::sleep_for(1);
		}
	}

	void flushLogBuffer() noexcept {
		if (!mLogFile) { return; }

		const auto snapshot{ mLogBuffer.snapshot(0, mLastFlushPos).fast() };
		if (snapshot.size() == 0) { mLastFlushTime = Millis::now(); return; }
		for (const auto& entry : snapshot) { mLogFile << entry << '\n'; }

		mLogFile.flush();
		mLastFlushPos += snapshot.size();
		mLastFlushTime = Millis::now();
	}

	void initLogFile(const std::string& filename, const std::string& directory) noexcept {
		if (filename.empty() || directory.empty()) {
			blog.newEntry<BLOG::ERROR>(
				"Log file name/path cannot be blank!");
			return;
		}

		const auto newPath{ fs::Path(directory) / filename };

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


public:
	~LoggerInstance() noexcept {
		if (mLogFlusherThread.joinable()) {
			mLogFlusherThread.request_stop();
			mLogFlusherThread.join();
		}
		flushLogBuffer();
	}

	auto& buffer() noexcept { return mLogBuffer; }
} ;



/*==================================================================*/

static constexpr std::string_view
getSeverityString(BLOG type) noexcept {
	switch (type) {
		case BLOG::INFO:  return "INFO";
		case BLOG::WARN:  return "WARN";
		case BLOG::ERROR: return "ERROR";
		case BLOG::FATAL: return "FATAL";
		case BLOG::DEBUG: return "DEBUG";
		default: return "UNKN";
	}
}

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

BasicLogger::BasicLogger() noexcept {
	static LoggerInstance logger;
	sMainLog = &logger;
}

void BasicLogger::initLogFile(const std::string& filename, const std::string& directory) noexcept {
	sMainLog->initLogFile(filename, directory);
}

template <BLOG LOG_SEVERITY>
void BasicLogger::newEntry_(const std::string& message) {
	using namespace std::chrono;

	sMainLog->buffer().push(fmt::format("{:%H:%M:%S} {:>5} > {}",
		milliseconds(Millis::now()), getSeverityString(LOG_SEVERITY), message));
}

template void BasicLogger::newEntry_<BLOG::INFO>(const std::string&);
template void BasicLogger::newEntry_<BLOG::WARN>(const std::string&);
template void BasicLogger::newEntry_<BLOG::ERROR>(const std::string&);
template void BasicLogger::newEntry_<BLOG::FATAL>(const std::string&);
template void BasicLogger::newEntry_<BLOG::DEBUG>(const std::string&);

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
