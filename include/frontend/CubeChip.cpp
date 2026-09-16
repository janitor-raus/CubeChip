/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDir.hpp"
#include "ThreadAffinity.hpp"
#include "BasicLogger.hpp"
#include "BasicInput.hpp"
#include "StringJoin.hpp"
#include "AttachConsole.hpp"

#include <cxxopts.hpp>

#include "ApplicationHost.hpp"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#  pragma warning(push)
#  pragma warning(disable : 5039)
#    include <mbctype.h>
#  pragma warning(pop)

#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

/*==================================================================*/

BasicLogger& blog = *BasicLogger::initialize();

/*==================================================================*/

SDL_AppResult SDL_AppInit(void **host, int argc, char *argv[]) {
	static_assert(std::endian::native == std::endian::little,
		"Only little-endian systems are supported!");

#ifdef _WIN32
	// Set the app locale and output code page to UTF-8,
	// so that we can handle Unicode paths and filenames.
	_setmbcp(CP_UTF8);
	setlocale(LC_CTYPE, ".UTF-8");
	SetConsoleOutputCP(CP_UTF8);
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

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

	HomeDir::init(
		result["homedir" ].as_optional<std::string>().value_or(""),
		result["portable"].as_optional<bool>().value_or(false),
		c_org_name, c_app_name
	);

	if (HomeDir::path().empty()) { return SDL_APP_FAILURE; }

	blog.create_log(std::to_string(
		thread_affinity::get_process_id()),
		HomeDir::path() + "logs"
	);

	*host = ApplicationHost::init_application(
		result["config"  ].as_optional<std::string>().value_or("settings.toml"),
		result["program" ].as_optional<std::string>().value_or(""),
		result["headless"].as_optional<bool>().value_or(false)
	);

	return *host ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

/*==================================================================*/

SDL_AppResult SDL_AppIterate(void* host_ptr) {
	auto* host = static_cast<ApplicationHost*>(host_ptr);

	BasicKeyboard::poll_global_state();
	return SDL_AppResult(host->process_client_frame());
}

/*==================================================================*/

SDL_AppResult SDL_AppEvent(void* host_ptr, SDL_Event* event) {
	auto* host = static_cast<ApplicationHost*>(host_ptr);

	return SDL_AppResult(host->handle_client_events(*event));
}

/*==================================================================*/

void SDL_AppQuit(void* host_ptr, SDL_AppResult) {
	auto* host = static_cast<ApplicationHost*>(host_ptr);

	host->quit_application();
	blog.shutdown();
}
