/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

/*==================================================================*/

#include <version>

#if defined(__cpp_lib_jthread) && (__cpp_lib_jthread >= 201911L)
	#include <thread>

	using Thread    = std::jthread;
	using StopToken = std::stop_token;
#else
	#include <jthread.hpp>
	#include <thread>

	using Thread    = nonstd::jthread;
	using StopToken = nonstd::stop_token;
#endif
