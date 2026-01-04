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
#include <unordered_map>

#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"
#include "ThreadAffinity.hpp"
#include "Millis.hpp"
#include "Thread.hpp"

/*==================================================================*/

static thread_local auto s_format_buffer = std::string(512, '\0');

static void standard_string_formatter_for_LogEntry(const LogEntry& entry) noexcept {
	s_format_buffer.clear();
	fmt::format_to(std::back_inserter(s_format_buffer), "{0})\t{1}\t{2}\t{3}\n",
		entry.index, NanoTime(entry.time).format_as_timer(),
		BLOG(entry.level).as_string(), entry.message);
}

static std::string s_log_file_path{};

static void touch_file_timestamp() noexcept {
	(void) fs::last_write_time(s_log_file_path, fs::Time::clock::now());
}

static std::atomic<unsigned> s_next_tid{ 1u };

static thread_local unsigned s_this_tid = s_next_tid \
	.fetch_add(1u, std::memory_order::relaxed);

/*==================================================================*/

static auto monotonicCount() noexcept {
	static std::atomic<std::uint32_t> counter = 1;
	return counter.fetch_add(1, std::memory_order::relaxed);
}

LogEntry::LogEntry(BLOG::LEVEL level, std::string message) noexcept
	: thread (s_this_tid)
	, index  (monotonicCount())
	, source (0) // XXX
	, level  (level)
	, time   (Millis::raw())
	, message(std::move(message))
{}

std::string LogEntry::as_string() const noexcept {
	::standard_string_formatter_for_LogEntry(*this);
	return s_format_buffer;
}

/*==================================================================*/

class BasicLoggerContext {
	friend class BasicLogger;

	using LogBuffer = BasicLogger::LogBuffer;

	std::ofstream m_log_file_handle;
	Thread m_log_flusher_thread;

	static constexpr std::size_t s_flush_interval_ms = 10000;

	std::size_t m_last_flush_pos{};
	std::size_t m_last_flush_time{};

	std::unordered_map<std::string, std::size_t>
		m_logger_sources_map{};

	LogBuffer m_log_backtrace_buffer;

	bool test_flush_size() const noexcept {
		const auto head = m_log_backtrace_buffer.head();
		return head >= m_last_flush_pos && \
			head - m_last_flush_pos >= (m_log_backtrace_buffer.size() / 2);
	}

	bool test_flush_time() const noexcept {
		return Millis::now() - m_last_flush_time >= s_flush_interval_ms;
	}

	void flush_log_backtrace_buffer() noexcept{
		if (!m_log_file_handle) { return; }

		const auto snapshot = m_log_backtrace_buffer.snapshot(0, m_last_flush_pos).fast();

		if (snapshot.size() == 0) {
			::touch_file_timestamp();
			m_last_flush_time = Millis::now();
			return;
		}

		for (const auto& entry : snapshot) {
			::standard_string_formatter_for_LogEntry(entry);
			m_log_file_handle.write(s_format_buffer.data(), s_format_buffer.size());
		}

		m_log_file_handle.flush();
		::touch_file_timestamp();

		m_last_flush_pos += snapshot.size();
		m_last_flush_time = Millis::now();
	}

	void create_log(const std::string& filename, const std::string& directory) noexcept {
		auto current_time = NanoTime(Millis::initial_wall());

		blog.newEntry<BLOG::INF>("Logging started on {}",
			current_time.format_as_datetime("{:%Y-%m-%d, at %H:%M:%S}"));

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
			.format_as_datetime("{:%Y-%m-%d__%H-%M-%S}__pid-") + filename + ".log")).string();

		m_log_file_handle.open(s_log_file_path, std::ios::trunc);
		if (!m_log_file_handle) {
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
		m_log_flusher_thread = Thread([&](StopToken token) noexcept {
			thread_affinity::set_affinity(0b11ull);

			do {
				if (test_flush_size() || test_flush_time())
					[[unlikely]] { flush_log_backtrace_buffer(); }
				Millis::sleep_for(1);
			}
			while (!token.stop_requested());
		});
	}

	~BasicLoggerContext() noexcept {
		if (m_log_flusher_thread.joinable()) {
			m_log_flusher_thread.request_stop();
			m_log_flusher_thread.join();
		}
		flush_log_backtrace_buffer();
	}

	auto buffer() const noexcept -> const auto& { return m_log_backtrace_buffer; }
	auto buffer()       noexcept ->       auto& { return m_log_backtrace_buffer; }
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
