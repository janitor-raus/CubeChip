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
#include "SimpleFileIO.hpp"
#include "ThreadAffinity.hpp"
#include "Millis.hpp"
#include "Thread.hpp"

/*==================================================================*/

static thread_local std::string sFormatBuffer(512, '\0');

static void standard_string_formatter_for_LogEntry(const LogEntry& entry) noexcept {
	sFormatBuffer.clear();
	fmt::format_to(std::back_inserter(sFormatBuffer), "{0}) {1} {3:>{2}} > {4}\n",
		entry.index, NanoTime(entry.time).format_as_timer(), BLOG::STR_LEN,
		BLOG(entry.level).as_string(), entry.message);
}

static std::string s_log_file_path{};

static void touch_file_timestamp() noexcept {
	(void) fs::last_write_time(s_log_file_path, fs::Time::clock::now());
}

/*==================================================================*/

static auto monotonicCount() noexcept {
	static std::atomic<std::uint32_t> counter = 1;
	return counter.fetch_add(1, std::memory_order::relaxed);
}

LogEntry::LogEntry(BLOG::LEVEL level, std::string message) noexcept
	: hash    (std::hash<std::thread::id>()(std::this_thread::get_id()))
	, time    (Millis::raw())
	, index   (monotonicCount())
	, level   (level)
	, message (std::move(message))
{}

std::string LogEntry::as_string() const noexcept {
	::standard_string_formatter_for_LogEntry(*this);
	return sFormatBuffer;
}

/*==================================================================*/

class BasicLoggerContext {
	friend class BasicLogger;

	using LogBuffer = BasicLogger::LogBuffer;

	std::ofstream mLogFile;
	Thread mFlusherThread;

	static constexpr std::size_t s_flush_interval_ms = 10000;

	std::size_t mLastFlushPos{};
	std::size_t mLastFlushTime{};

	LogBuffer mLogBuffer;

	bool testFlushSize() const noexcept {
		const auto head = mLogBuffer.head();
		return head >= mLastFlushPos && \
			head - mLastFlushPos >= (mLogBuffer.size() / 2);
	}

	bool testFlushTime() const noexcept {
		return Millis::now() - mLastFlushTime >= s_flush_interval_ms;
	}

	void flushLogBuffer() noexcept{
		if (!mLogFile) { return; }

		const auto snapshot = mLogBuffer.snapshot(0, mLastFlushPos).fast();

		if (snapshot.size() == 0) {
			::touch_file_timestamp();
			mLastFlushTime = Millis::now();
			return;
		}

		for (const auto& entry : snapshot) {
			::standard_string_formatter_for_LogEntry(entry);
			mLogFile.write(sFormatBuffer.data(), sFormatBuffer.size());
		}

		mLogFile.flush();
		::touch_file_timestamp();

		mLastFlushPos += snapshot.size();
		mLastFlushTime = Millis::now();
	}

	void create_log(const std::string& filename, const std::string& directory) noexcept {
		auto current_time = NanoTime(Millis::initial_wall());

		blog.newEntry<BLOG::INF>("Logging started on {}",
			current_time.format_as_datetime("{:%Y-%m-%d %H:%M:%S}"));

		if (filename.empty() || directory.empty()) {
			blog.newEntry<BLOG::ERR>(
				"Log file name/path cannot be blank!");
			return;
		}

		if (!fs::create_directories(directory)) {
			blog.newEntry<BLOG::ERR>(
				"Failed to create subfolder structure for Log file: \"{}\"", directory);
			return;
		}

		s_log_file_path = (std::filesystem::path(directory) / (current_time \
			.format_as_datetime("{:%Y-%m-%d_%H-%M-%S}_") + filename + ".log")).string();

		mLogFile.open(s_log_file_path, std::ios::trunc);
		if (!mLogFile) {
			blog.newEntry<BLOG::ERR>(
				"Unable to create new Log file: \"{}\"",
				std::exchange(s_log_file_path, {}));
		}

		const auto cutoff_time = fs::Time::clock::now() - std::chrono::days(7);

		fs::for_each_dir_entry(directory, [&](auto& entry, auto& ec) noexcept -> bool {
			if (ec) { return false; }

			auto regular_file = entry.is_regular_file(ec);
			if (ec || !regular_file) { return false; }

			if (entry.path().extension() != ".log") { return false; }

			auto last_write = entry.last_write_time(ec);
			if (ec || last_write > cutoff_time) { return false; }

			(void) fs::remove(entry.path());
			return false;
		});
	}

public:
	BasicLoggerContext() noexcept {
		mFlusherThread = Thread([&](StopToken token) noexcept {
			thread_affinity::set_affinity(0b11ull);

			do {
				if (testFlushSize() || testFlushTime())
					[[unlikely]] { flushLogBuffer(); }
				Millis::sleep_for(1);
			}
			while (!token.stop_requested());
		});
	}

	~BasicLoggerContext() noexcept {
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
	: m_context(std::make_unique<BasicLoggerContext>())
{}

BasicLogger::~BasicLogger() noexcept = default;

void BasicLogger::shutdown() noexcept {
	if (m_context) { m_context.reset(); }
}

auto BasicLogger::buffer() const noexcept -> const LogBuffer* {
	return m_context ? &m_context->buffer() : nullptr;
}

void BasicLogger::create_log(const std::string& filename, const std::string& directory) noexcept {
	if (m_context) { m_context->create_log(filename, directory); }
}

auto BasicLogger::get_log_path() const noexcept -> std::string {
	return s_log_file_path;
}

template <BLOG::LEVEL LOG_LEVEL>
void BasicLogger::newEntry_(std::string&& message) noexcept {
	if (m_context) { m_context->buffer().push(LogEntry(LOG_LEVEL, message)); }
}

template void BasicLogger::newEntry_<BLOG::DBG>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::INF>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::WRN>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::ERR>(std::string&&) noexcept;
template void BasicLogger::newEntry_<BLOG::FTL>(std::string&&) noexcept;

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
