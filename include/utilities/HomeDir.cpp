/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDir.hpp"

#include "SimpleFileIO.hpp"
#include "PathGetters.hpp"

#include <string>

/*==================================================================*/

#include <SDL3/SDL_messagebox.h>
#include <fmt/format.h>

template <typename... T>
static void println_fatal(fmt::format_string<T...> fmt_str, T&&... args) noexcept {
	std::string message = fmt::format(fmt_str, std::forward<T>(args)...);
	fmt::println(stderr, "{}", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
		"Fatal Initialization Error", message.c_str(), nullptr);
}

/*==================================================================*/

static std::string s_home_path{};

std::string_view HomeDir::path() noexcept {
	return s_home_path;
}

void HomeDir::init(
	std::string_view home_override, bool portable,
	std::string_view org, std::string_view app
) noexcept {
	// Only allow setting the home path once
	if (!s_home_path.empty()) { return; }

	// 1) Check for explicit home directory override
	if (!home_override.empty()) {
		const auto writable_dir = fs::is_writable_directory(home_override);
		if (writable_dir.value_or(false)) {
			// Sanitize the path to ensure it ends with a directory separator
			fmt::println("Home directory (--homedir) has been successfully re-routed"
				" to '{}'", (s_home_path = (fs::Path(home_override) / "").string()));
			return;
		} else {
			::println_fatal("Failed to re-route Home directory (--homedir) to '{}': {}", home_override,
				writable_dir.error() ? writable_dir.error().message() : "not a directory or doesn't exist!");
			return;
		}
	}

	// 2) Check for explicit portable mode override
	if (portable) {
		const auto writable_dir = fs::is_writable_directory(::get_base_path());
		if (writable_dir.value_or(false)) {
			fmt::println("Home directory (--portable) has been successfully "
				"re-routed to '{}'.", (s_home_path = ::get_base_path()));
			return;
		} else {
			::println_fatal("Failed to re-route Home directory (--portable) to '{}': {}", ::get_base_path(),
				writable_dir.error() ? writable_dir.error().message() : "cannot write to directory!");
			return;
		}
	}

	// 3) Check for portable.txt in the base path
	if (!::get_base_path().empty()) {
		if (fs::exists(::get_base_path() / "portable.txt").value_or(false)) {
			const auto writable_dir = fs::is_writable_directory(::get_base_path());
			if (writable_dir.value_or(false)) {
				fmt::println("Home directory (portable.txt) has been successfully "
					"re-routed to '{}'.", (s_home_path = ::get_base_path()));
				return;
			} else {
				::println_fatal("Failed to re-route Home directory (portable.txt) to '{}': {}\n"
					"Ignoring error and falling back to default Home directory detection!", ::get_base_path(),
					writable_dir.error() ? writable_dir.error().message() : "cannot write to directory!");
			}
		}
	}

	// 4) Default home path detection
	const auto writable_dir = fs::is_writable_directory(::get_home_path(
		org.empty() ? nullptr : org.data(),
		app.empty() ? nullptr : app.data()
	));
	if (writable_dir.value_or(false)) {
		fmt::println("Home directory has been successfully "
			"set to '{}'.", (s_home_path = ::get_home_path()));
	} else {
		::println_fatal("Failed to determine if directory '{}' is writable: {}", ::get_home_path(),
			writable_dir.error() ? writable_dir.error().message() : "not a directory or doesn't exist!");
	}
}
