/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "AtomSharedPtr.hpp"
#include "TripleBuffer.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "SettingWrapper.hpp"
#include "EzMaths.hpp"

#include <functional>

/*==================================================================*/
	#pragma region BasicVideoSpec Singleton Class

class BasicVideoSpec final {

	SDL_Unique<SDL_Window>   mMainWindow{};
	SDL_Unique<SDL_Renderer> mMainRenderer{};

/*==================================================================*/

public:
	struct Settings {
		static constexpr ez::Rect
			defaults{ 0, 0, 640, 480 };

		ez::Rect window = defaults;
		bool first_run = true;

		SettingsMap map() noexcept;
	};

	[[nodiscard]]
	auto exportSettings() const noexcept -> Settings;

private:
	BasicVideoSpec(const Settings& settings, bool& success) noexcept;
	~BasicVideoSpec() noexcept;
	BasicVideoSpec(const BasicVideoSpec&) = delete;
	BasicVideoSpec& operator=(const BasicVideoSpec&) = delete;

/*==================================================================*/

public:
	static auto* initialize(const Settings& settings) noexcept {
		static bool sInitSuccess = true;
		static BasicVideoSpec self(settings, sInitSuccess);
		return sInitSuccess ? &self : nullptr;
	}

/*==================================================================*/

public:
	float getDisplayRefreshRate(SDL_DisplayID display) noexcept;
	void  normalizeRectToDisplay(ez::Rect& rect, ez::Rect& deco, bool first_run) noexcept;

/*==================================================================*/

	SDL_Window* getMainWindow() const noexcept
		{ return mMainWindow; }

	SDL_Renderer* getMainRenderer() const noexcept
		{ return mMainRenderer; }

/*==================================================================*/

public:
	static SDL_Texture* makeDisplayTexture(SDL_Renderer* renderer, int w, int h) noexcept;
	static void renderStreamTexture(SDL_Renderer* renderer,
		SDL_Texture* texture, const std::byte* src_buffer) noexcept;

public:
	void setMainWindowTitle(const std::string& title) noexcept;
	bool isMainWindowID(unsigned id) const noexcept;
	void raiseMainWindow() noexcept;

	bool renderPresent() noexcept;
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
