/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <fmt/chrono.h>
#include <cstdint>
#include <string_view>

#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"
#include "SlidingRingBuffer.hpp"
#include "ThreadAffinity.hpp"
#include "Thread.hpp"
#include "Millis.hpp"

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

static auto monotonicCount() noexcept {
	static Atom<std::uint32_t> counter{};
	return counter.fetch_add(1, mo::relaxed);
}

/*==================================================================*/

template <typename LogLevelT>
	requires (sizeof(LogLevelT) <= sizeof(int))
struct alignas(HDIS) LogEntry {
	std::uint64_t hash{};  // thread hash, to be filled in automatically
	std::uint64_t time{};  // timestamp, expected to be ns
	std::uint32_t index{}; // entry index, expected to be monotonic
	LogLevelT     level{}; // log level, usually tied to an enum
	const void* userdata{}; // pointer to additional data, if any
	std::string message{};  // self-explanatory

	constexpr LogEntry() noexcept = default;
	LogEntry(std::uint32_t index, LogLevelT level, const void* userdata, std::string message) noexcept
		: hash { std::hash<std::thread::id>{}(std::this_thread::get_id()) }
		, time { static_cast<std::uint64_t>(Millis::raw()) }
		, index{ index }
		, level{ level }
		, userdata{ userdata }
		, message { std::move(message) }
	{}
};

class LoggerInstance {
	friend class BasicLogger;

	SlidingRingBuffer
		<LogEntry<BLOG>, 512> mLogBuffer;

	alignas(HDIS)
	std::ofstream mLogFile;

	std::size_t mLastFlushPos{};
	std::size_t mLastFlushTime{};
	Thread mLogFlusherThread;

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
		using namespace std::chrono;
		if (!mLogFile) { return; }

		const auto snapshot{ mLogBuffer.snapshot(0, mLastFlushPos).fast() };
		if (snapshot.size() == 0) { mLastFlushTime = Millis::now(); return; }
		for (const auto& entry : snapshot) {
			mLogFile << fmt::format("{}) {:%H:%M:%S} {:>5} > {}", entry.index,
				duration_cast<milliseconds>(nanoseconds(entry.time)),
				getSeverityString(entry.level), entry.message) << '\n';
		}

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
	#pragma region BasicLogger Singleton Class

BasicLogger::BasicLogger() noexcept {
	static LoggerInstance logger;
	sMainLog = &logger;
}

void BasicLogger::initLogFile(const std::string& filename, const std::string& directory) noexcept {
	sMainLog->initLogFile(filename, directory);
}

template <BLOG LOG_LEVEL>
void BasicLogger::newEntry_(std::string&& message) {
	sMainLog->buffer().push(LogEntry(monotonicCount(),
		LOG_LEVEL, nullptr, std::move(message)));
}

template void BasicLogger::newEntry_<BLOG::INFO> (std::string&& message);
template void BasicLogger::newEntry_<BLOG::WARN> (std::string&& message);
template void BasicLogger::newEntry_<BLOG::ERROR>(std::string&& message);
template void BasicLogger::newEntry_<BLOG::FATAL>(std::string&& message);
template void BasicLogger::newEntry_<BLOG::DEBUG>(std::string&& message);

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
