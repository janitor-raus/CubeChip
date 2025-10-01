/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <fmt/chrono.h>
#include <utility>
#include <string_view>
#include <filesystem>

#include "Millis.hpp"
#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"
#include "SimpleRingBuffer.hpp"

/*==================================================================*/

std::filesystem::path sLogPath{};

static SimpleRingBuffer
	<std::string, 512> sLogBuffer;

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

bool BasicLogger::initLogFile(const std::string& filename, const std::string& directory) noexcept {
	if (filename.empty() || directory.empty()) {
		newEntry<BLOG::ERROR>("Log file name/path cannot be blank!");
		return false;
	}

	const auto newPath{ std::filesystem::path(directory) / filename };
	const auto tmpPath{ std::filesystem::path(directory) / (filename + ".tmp") };

	if (std::ofstream logFile{ tmpPath })
		{ logFile.close(); }
	else {
		newEntry<BLOG::ERROR>("Unable to create new Log file: \"{}\"",
			tmpPath.string());
		return false;
	}

	const auto fileRename{ fs::rename(tmpPath, newPath) };
	if (!fileRename) {
		newEntry<BLOG::ERROR>(
			"Unable to replace old Log file: \"{}\" [{}]",
			newPath.string(), fileRename.error().message());
		return false;
	}

	sLogPath.assign(newPath);
	newEntry<BLOG::INFO>("Logging started on {:%Y-%m-%d %H:%M:%S}",
		std::chrono::system_clock::now());
	return true;
}

void BasicLogger::flushToDisk(std::size_t count) noexcept {
	if (!sLogPath.empty()) {
		if (count == 0) { count = sLogBuffer.size(); }
		count = std::min(count, sLogBuffer.size());
		std::ofstream logFile(sLogPath, std::ios::app);

		if (logFile) {
			for (const auto& entry : sLogBuffer.safe_snapshot_asc(/*count*/))
				{ logFile << entry << '\n'; }
		}
	}
}

/*==================================================================*/

template <BLOG LOG_SEVERITY>
void BasicLogger::writeEntry(const std::string& message) {
	using namespace std::chrono;

	sLogBuffer.push(fmt::format("{:%H:%M:%S} {:>5} > {}",
		duration_cast<milliseconds>(nanoseconds(Millis::raw())),
		getSeverityString(LOG_SEVERITY), message));
}

template void BasicLogger::writeEntry<BLOG::INFO>(const std::string&);
template void BasicLogger::writeEntry<BLOG::WARN>(const std::string&);
template void BasicLogger::writeEntry<BLOG::ERROR>(const std::string&);
template void BasicLogger::writeEntry<BLOG::FATAL>(const std::string&);
template void BasicLogger::writeEntry<BLOG::DEBUG>(const std::string&);

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
