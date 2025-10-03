/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "AtomSharedPtr.hpp"

/*==================================================================*/

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

class FrontendInterface {

public:
	static inline Atom<void(*)()>
		FnHook_OpenFile{};

	static inline Atom<void(*)()>
		FnHook_OpenDataDir{};

public:
	static void Initialize(SDL_Window*, SDL_Renderer*);
	static void Shutdown();

	static void ProcessEvent(void* event);
	static void NewFrame();
	static void RenderFrame(SDL_Renderer*);

	static float GetFrameHeight();

	static void UpdateFontScale(const void* data, int size, float scale);
	template <typename T, std::size_t N>
	static void UpdateFontScale(T(&appFont)[N], float scale)
		{ UpdateFontScale(appFont, N, scale); }

	static void PrepareViewport(
		bool enable, bool integer_scaling,
		int width, int height, int rotation,
		const char* overlay, SDL_Texture* texture
	);
	static void PrepareGeneralUI();
};
