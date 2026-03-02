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
#include <vector>
#include <unordered_map>

#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"
#include "ThreadAffinity.hpp"
#include "Millis.hpp"
#include "Thread.hpp"

/*==================================================================*/

struct StringHash {
	using is_transparent = void;

	size_t operator()(std::string_view sv) const noexcept {
		return std::hash<std::string_view>{}(sv);
	}
};

struct StringEq {
	using is_transparent = void;

	bool operator()(std::string_view a, std::string_view b) const noexcept {
		return a == b;
	}
};

/*==================================================================*/

struct LogSource {
	std::unordered_map<std::string, std::uint32_t, StringHash, StringEq>
		table{};

	std::atomic<std::uint32_t>
		count{}; // sync fence for m_names

	std::mutex
		guard{};

	std::vector<std::string>
		names{};

public:
	static auto* initialize() noexcept {
		static LogSource instance{};
		return &instance;
	}

private:
	LogSource() noexcept = default;
};

static LogSource* s_logger_sources = nullptr;

std::uint32_t get_source_index(std::string_view src_name) noexcept {
	if (src_name.empty()) { return 0; }
	std::scoped_lock lock(s_logger_sources->guard);

	auto it = s_logger_sources->table.find(src_name);
	if (it != s_logger_sources->table.end()) { return it->second; }

	const auto dest_index = s_logger_sources->count.load(std::memory_order::relaxed);

	s_logger_sources->table.emplace(src_name, dest_index);
	s_logger_sources->names.emplace_back(src_name);
	s_logger_sources->count.store(dest_index + 1, std::memory_order::release);

	return dest_index;
}

std::string get_source_name(std::uint32_t src_id) noexcept {
	return src_id < s_logger_sources->count.load(std::memory_order::acquire)
		? s_logger_sources->names[src_id] : std::string();
}

static thread_local std::uint32_t s_this_source_id = 0;

/*==================================================================*/

ScopedLogSource::ScopedLogSource(std::string_view src_name) noexcept
	: m_prev_source_id(s_this_source_id)
{
	s_this_source_id = ::get_source_index(src_name);
}

ScopedLogSource::~ScopedLogSource() noexcept {
	s_this_source_id = m_prev_source_id;
}

/*==================================================================*/

[[nodiscard]] static
auto& get_format_buffer() noexcept {
	static thread_local auto s_format_buffer = std::string(512, '\0');
	return s_format_buffer;
}

static void standard_string_formatter_for_LogEntry(const LogEntry& entry) noexcept {
	get_format_buffer().clear();
	fmt::format_to(std::back_inserter(get_format_buffer()), "{0})\t{1}\t{2}\t{3}\t{4}\t{5}\n",
		entry.index, entry.thread, NanoTime(entry.time).format_as_timer(),
		::get_source_name(entry.source), BLOG(entry.level).as_string(), entry.message);
}

static std::string s_log_file_path{};

static void touch_file_timestamp() noexcept {
	(void) fs::last_write_time(s_log_file_path, fs::Time::clock::now());
}

/*==================================================================*/

[[nodiscard]] static
auto get_current_tid() noexcept {
	static std::atomic<std::uint32_t> s_next_tid = 1u;
	static thread_local auto s_this_tid = s_next_tid \
		.fetch_add(1u, std::memory_order::relaxed);
	return s_this_tid;
}

[[nodiscard]] static
auto monotonic_count() noexcept {
	static std::atomic<std::uint32_t> counter = 1u;
	return counter.fetch_add(1, std::memory_order::acq_rel);
}

LogEntry::LogEntry(BLOG::LEVEL level, std::uint32_t source, std::string message) noexcept
	: thread (get_current_tid())
	, index  (monotonic_count())
	, source (source)
	, level  (level)
	, time   (Millis::raw())
	, message(std::move(message))
{}

std::string LogEntry::as_string() const noexcept {
	::standard_string_formatter_for_LogEntry(*this);
	return get_format_buffer();
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
			m_log_file_handle.write(
				get_format_buffer().data(),
				get_format_buffer().size()
			);
		}

		m_log_file_handle.flush();
		::touch_file_timestamp();

		m_last_flush_pos += snapshot.size();
		m_last_flush_time = Millis::now();
	}

	void create_log(const std::string& filename, const std::string& directory) noexcept {
		auto current_time = NanoTime(Millis::initial_wall());

		blog.info("Logging started on {}",
			current_time.format_as_datetime("{:%Y-%m-%d, at %H:%M:%S}"));

		if (filename.empty() || directory.empty()) {
			blog.error("Log file name/path cannot be blank!");
			return;
		}

		if (!fs::create_directories(directory)) {
			blog.error("Failed to create subfolder structure for Log file:"
				" \"{}\"", directory);
			return;
		}

		s_log_file_path = (std::filesystem::path(directory) / (current_time \
			.format_as_datetime("{:%Y-%m-%d__%H-%M-%S}__pid-") + filename + ".log")).string();

		m_log_file_handle.open(s_log_file_path, std::ios::trunc);
		if (!m_log_file_handle) {
			blog.error("Unable to create new Log file:"
				" \"{}\"", std::exchange(s_log_file_path, {}));
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
{
	s_logger_sources = LogSource::initialize();
	(void) ::get_source_index("global");
}

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

void BasicLogger::push_entry(BLOG::LEVEL level, std::string&& message) noexcept {
	if (m_context) { m_context->buffer().push(LogEntry(level, s_this_source_id, std::move(message))); }
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
