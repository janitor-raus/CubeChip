/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BasicLogger.hpp"
#include "AttachConsole.hpp"

#include <cxxopts.hpp>

#include "FrontendHost.hpp"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_version.h>

#ifdef _WIN32
	#pragma warning(push)
	#pragma warning(disable : 5039)
		#include <mbctype.h>
	#pragma warning(pop)

	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#endif

/*==================================================================*/

BasicLogger& blog = *BasicLogger::initialize();

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
	SDL_SetHint(SDL_HINT_APP_NAME, c_app_name);
	SDL_SetAppMetadata(c_app_name, c_app_ver.with_hash, nullptr);

	cxxopts::Options options(c_app_name, "Cross-platform multi-system emulator");

	{
		options.add_options("Runtime")
			("program",  "Force application to load a program on startup.",
				cxxopts::value<std::string>())
			("headless", "Force application to run without a graphical user interface (stub).",
				cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

		options.add_options("Configuration")
			("homedir",  "Force application to use a different home directory to read/write files. Takes precedence over --portable.",
				cxxopts::value<std::string>())
			("config",   "Force application to use a different config file to load/save settings, relative to the home directory.",
				cxxopts::value<std::string>())
			("portable", "Force application to operate in portable mode, setting the home directory to the executable's location. Overridden by --homedir.",
				cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

		options.add_options("General")
			("version", "Print application version info.")
			("help",    "List application options.");

		options.parse_positional({ "program" });
		options.positional_help("program_file");
	}

	cxxopts::ParseResult result;
	try { result = options.parse(argc, argv); }
	catch (const cxxopts::exceptions::exception& e) {
		console::attach();
		fmt::println(stderr, "Error parsing options: {}", e.what());
		fmt::println(stderr, "Use --help to list options.");
		return SDL_APP_FAILURE;
	}

	if (result.count("version")) {
		console::attach();
		fmt::println("{} compiled on: {} ({})", c_app_name,
			c_app_ver.with_date, c_app_ver.ghash);

		return SDL_APP_SUCCESS;
	}

	if (result.count("help")) {
		console::attach();
		fmt::println("{}", options.help({ "Runtime", "Configuration", "General" }));

		return SDL_APP_SUCCESS;
	}

	*Host = FrontendHost::init_application(
		result["homedir"].as_optional<std::string>().value_or(""),
		result["config" ].as_optional<std::string>().value_or(""),
		result["program"].as_optional<std::string>().value_or(""),
		result["program"].as_optional<bool>().value_or(false)
	);

	return *Host ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

/*==================================================================*/

SDL_AppResult SDL_AppIterate(void *pHost) {
	auto* Host = static_cast<FrontendHost*>(pHost);

	return SDL_AppResult(Host->process_client_frame());
}

/*==================================================================*/

SDL_AppResult SDL_AppEvent(void *pHost, SDL_Event *event) {
	auto* Host = static_cast<FrontendHost*>(pHost);

	return SDL_AppResult(Host->handle_client_events(event));
}

/*==================================================================*/

void SDL_AppQuit(void* pHost, SDL_AppResult) {
	auto* Host = static_cast<FrontendHost*>(pHost);

	Host->quit_application();
	blog.shutdown();
}
