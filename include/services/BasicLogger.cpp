/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <utility>

#include "BasicLogger.hpp"
#include "SimpleFileIO.hpp"

#include <SDL3/SDL_messagebox.h>

/*==================================================================*/
	#pragma region BasicLogger Singleton Class

bool BasicLogger::initLogFile(const Str& filename, const Path& directory) noexcept {
	if (filename.empty() || directory.empty()) {
		newEntry(BLOG::ERROR, "Log file name/path cannot be blank!");
		return false;
	}

	const auto newPath{ directory / filename };
	const auto tmpPath{ directory / (filename + ".tmp") };

	if (std::ofstream logFile{ tmpPath })
		{ logFile.close(); }
	else {
		newEntry(BLOG::ERROR,
			"Unable to create new Log file: \"{}\"", tmpPath.string());
		return false;
	}

	const auto fileRename{ fs::rename(tmpPath, newPath) };
	if (!fileRename) {
		newEntry(BLOG::ERROR,
			"Unable to replace old Log file: \"{}\" [{}]",
			newPath.string(), fileRename.error().message());
		return false;
	}

	mLogPath.assign(newPath);
	return true;
}

/*==================================================================*/

StrV BasicLogger::getSeverity(BLOG type) const noexcept {
	switch (type) {
		case BLOG::INFO:
			return "INFO";

		case BLOG::WARN:
			return "WARN";

		case BLOG::ERROR:
			return "ERROR";

		case BLOG::FATAL:
			return "FATAL";

		case BLOG::DEBUG:
			return "DEBUG";

		default:
			return "OTHER";
	}
}

void BasicLogger::writeEntry(BLOG type, const Str& message) {
	auto output{ fmt::format("{} :: {}", getSeverity(type), message) };

	mLogBuffer.push(output);
	if (mLogPath.empty()) { fmt::println("{}", output); }
	else {
		std::ofstream logFile(mLogPath, std::ios::app);
		if (logFile) { logFile << output << std::endl; }
		else {
			newEntry(BLOG::ERROR, "Unable to write to Log file: \"{}\"",
				std::move(mLogPath).string());
			fmt::println(stderr, "{}", output);
		}
	}
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
