/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BasicLogger.hpp"
#include "ThreadAffinity.hpp"
#include "AttachConsole.hpp"

#include <cxxopts.hpp>

#include "FrontendHost.hpp"

#ifdef _WIN32
	#pragma warning(push)
	#pragma warning(disable : 5039)
		#include <mbctype.h>
	#pragma warning(pop)
	#define NOMINMAX
	#include <windows.h>
#endif

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_version.h>

/*==================================================================*/

BasicLogger& blog{ *BasicLogger::initialize() };

/*==================================================================*/

SDL_AppResult SDL_AppInit(void **Host, int argc, char *argv[]) {
	static_assert(std::endian::native == std::endian::little,
		"Only little-endian systems are supported!");

#ifdef _WIN32
	_setmbcp(CP_UTF8);
	setlocale(LC_CTYPE, ".UTF-8");
	SetConsoleOutputCP(CP_UTF8);
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	SDL_SetHint(SDL_HINT_APP_NAME, AppName);
	SDL_SetAppMetadata(AppName, AppVer.with_hash, nullptr);

	cxxopts::Options options(AppName, "Cross-platform multi-system emulator");

	options.add_options("Runtime")
		("program",  "Forces the application to load a program on startup.",
			cxxopts::value<Str>())
		("headless", "Forces the application to run without a graphical user interface.",
			cxxopts::value<bool>()->default_value("false"));

	options.add_options("Configuration")
		("homedir",  "Forces application to use a different home directory to read/write files.",
			cxxopts::value<Str>())
		("config",   "Forces application to use a different config file to load/save settings, relative to the home directory.",
			cxxopts::value<Str>())
		("portable", "Force application to operate in portable mode, setting the home directory to the executable's location. Overriden by --home.",
			cxxopts::value<bool>()->default_value("false"));

	options.add_options("General")
		("version", "Print application version info.")
		("help",    "List application options.");

	options.parse_positional({ "program" });
	options.positional_help("program_file");

	auto result{ options.parse(argc, argv) };

	if (result.count("version")) {
		Console::Attach();
		fmt::println("{} compiled on: {} ({})",
			AppName, AppVer.with_date, AppVer.ghash);

		return SDL_APP_SUCCESS;
	}

	if (result.count("help")) {
		Console::Attach();
		fmt::println("{}", options.help({ "Runtime", "Configuration", "General" }));

		return SDL_APP_SUCCESS;
	}

	if (!FrontendHost::initApplication(
		result.count("homedir")  ? result["homedir"].as<Str>() : ""s,
		result.count("config")   ? result["config"] .as<Str>() : ""s,
		result.count("portable") ? true : false
	)) { return SDL_APP_FAILURE; }

	*Host = FrontendHost::initialize(result.count("program") ? result["program"].as<Str>() : ""s);

	thread_affinity::set_affinity(0b11ull);

	return SDL_APP_CONTINUE;
}

/*==================================================================*/

SDL_AppResult SDL_AppIterate(void *pHost) {
	auto* Host{ static_cast<FrontendHost*>(pHost) };

	Host->processFrame();

	return SDL_APP_CONTINUE;
}

/*==================================================================*/

SDL_AppResult SDL_AppEvent(void *pHost, SDL_Event *event) {
	auto* Host{ static_cast<FrontendHost*>(pHost) };

	return SDL_AppResult(Host->processEvents(event));
}

/*==================================================================*/

void SDL_AppQuit(void *pHost, SDL_AppResult) {
	if (pHost) {
		auto* Host{ static_cast<FrontendHost*>(pHost) };
		Host->quitApplication();
	}
	blog.flushToDisk();
}
