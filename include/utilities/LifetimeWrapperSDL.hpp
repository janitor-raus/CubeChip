/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "FriendlyUnique.hpp"

/*==================================================================*/

#define USE_FRIENDLY_UNIQUE

template <typename T>
struct SDL_Deleter;

#ifdef USE_FRIENDLY_UNIQUE
	#include "FriendlyUnique.hpp"

	template <typename T>
	using SDL_Unique = FriendlyUnique<T, SDL_Deleter<T>>;
#else
	#include <memory>
	template <typename T>
	using SDL_Unique = std::unique_ptr<T, SDL_Deleter<T>>;
#endif // USE_FRIENDLY_UNIQUE

/*==================================================================*/

struct SDL_Window;
template <> struct SDL_Deleter<SDL_Window>
	{ void operator()(SDL_Window*) const noexcept; };

struct SDL_Renderer;
template <> struct SDL_Deleter<SDL_Renderer>
	{ void operator()(SDL_Renderer*) const noexcept; };

struct SDL_Texture;
template <> struct SDL_Deleter<SDL_Texture>
	{ void operator()(SDL_Texture*) const noexcept; };

struct SDL_AudioStream;
template <> struct SDL_Deleter<SDL_AudioStream>
	{ void operator()(SDL_AudioStream*) const noexcept; };

using SDL_DisplayID     = unsigned;
using SDL_AudioDeviceID = unsigned;
using SDL_JoystickID    = unsigned;
template <> struct SDL_Deleter<unsigned>
	{ void operator()(unsigned*) const noexcept; };

template <> struct SDL_Deleter<char>
	{ void operator()(char*) const noexcept; };

template <> struct SDL_Deleter<const char>
	{ void operator()(const char*) const noexcept; };

/*==================================================================*/

namespace sdl {
	template <typename T>
	[[nodiscard]] constexpr
	auto make_unique(T* ptr) noexcept
		{ return SDL_Unique<T>(ptr); }
}
