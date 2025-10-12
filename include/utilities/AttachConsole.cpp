/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "AttachConsole.hpp"

/*==================================================================*/

#ifdef _WIN32
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
	#include <iostream>

	void Console::Attach() noexcept {
		if (GetConsoleWindow()) { return; }

		if (!AttachConsole(ATTACH_PARENT_PROCESS))
			{ if (!AllocConsole()) { return; } }

		FILE* dummy;
		freopen_s(&dummy, "CONIN$",  "r", stdin);
		freopen_s(&dummy, "CONOUT$", "w", stdout);
		freopen_s(&dummy, "CONOUT$", "w", stderr);

		std::ios::sync_with_stdio(true);
	}

	void Console::Show() noexcept {
		ShowWindow(GetConsoleWindow(), SW_SHOW);
	}

	void Console::Hide() noexcept {
		ShowWindow(GetConsoleWindow(), SW_HIDE);
	}

#else
	void Console::Attach() noexcept {}
	void Console::Show()   noexcept {}
	void Console::Hide()   noexcept {}
#endif
