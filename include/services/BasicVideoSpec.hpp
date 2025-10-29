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

/*==================================================================*/
	#pragma region BasicVideoSpec Singleton Class

class BasicVideoSpec final {

public:
	struct Viewport {
		ez::Frame frame{};
		int multi{}, pxpad{};

		constexpr Viewport(int w = 0, int h = 0, int multi = 0, int pxpad = 0) noexcept
			: frame{ std::clamp(w, 0x0, 0xFFF), std::clamp(h, 0x0, 0xFFF) }
			, multi{ std::clamp(multi, 0x1, 0xF) }
			, pxpad{ std::clamp(pxpad, 0x0, 0xF) }
		{}

		constexpr auto rotate_if(bool cond) const noexcept {
			return cond
				? Viewport{ frame.h, frame.w, multi, pxpad }
				: Viewport{ frame.w, frame.h, multi, pxpad };
		}

		constexpr auto scaled() const noexcept {
			return ez::Frame{ frame.w * multi, frame.h * multi };
		}
		constexpr auto padded() const noexcept {
			return ez::Frame{ frame.w * multi + pxpad * 2, frame.h * multi + pxpad * 2 };
		}

		static constexpr auto pack(int w, int h, int multi, int pxpad) noexcept {
			return ((unsigned(w)     & 0xFFFu) <<  0) |
				   ((unsigned(h)     & 0xFFFu) << 12) |
				   ((unsigned(multi) & 0xFu)   << 24) |
				   ((unsigned(pxpad) & 0xFu)   << 28);
		}

		static constexpr auto unpack(unsigned packed) noexcept {
			return Viewport{
				int((packed >>  0) & 0xFFFu),
				int((packed >> 12) & 0xFFFu),
				int((packed >> 24) & 0xFu),
				int((packed >> 28) & 0xFu)
			};
		}
	};

private:
	SDL_Unique<SDL_Window>   mMainWindow{};
	SDL_Unique<SDL_Renderer> mMainRenderer{};
	SDL_Unique<SDL_Texture>  mWindowTexture{};
	SDL_Unique<SDL_Texture>  mSystemTexture{};

/*==================================================================*/

	Viewport       mCurViewport{};
	Atom<unsigned> mNewViewport{};

	Atom<unsigned> mOutlineColor{};
	Atom<ez::u8>   mTextureAlpha{ 0xFF };

	bool mUsingScanlines{};
	bool mIntegerScaling{};

	int mViewportRotation{};
	int mViewportScaleMode{};

public:
	TripleBuffer<unsigned> displayBuffer;

	struct Settings {
		static constexpr ez::Rect
			defaults{ 0, 0, 640, 480 };

		ez::Rect window{ defaults };
		struct Viewport {
			int  filtering{ 0 };
			bool int_scale{ true };
			bool scanlines{ true };
		} viewport;
		bool first_run{ true };

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
		static bool sInitSuccess{ true };
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

	bool isIntegerScaling()     const noexcept { return mIntegerScaling; }
	void isIntegerScaling(bool state) noexcept { mIntegerScaling = state; }
	void toggleIntegerScaling()       noexcept { mIntegerScaling = !mIntegerScaling; }

	bool isUsingScanlines()     const noexcept { return mUsingScanlines; }
	void isUsingScanlines(bool state) noexcept { mUsingScanlines = state; }
	void toggleUsingScanlines()       noexcept { mUsingScanlines = !mUsingScanlines; }

	void rotateViewport(int delta) noexcept {
		mViewportRotation += delta;
		mViewportRotation &= 3;
	}
	void setViewportRotation(int value) noexcept {
		mViewportRotation = value & 3;
	}

	auto getViewportScaleMode() const noexcept { return mViewportScaleMode; }
	void setViewportScaleMode(int mode) noexcept;
	void cycleViewportScaleMode() noexcept;
	void setBorderColor(unsigned color) noexcept;

/*==================================================================*/

private:
	void prepareWindowTexture() noexcept(false);
	void prepareSystemTexture() noexcept(false);
	void renderViewport() noexcept(false);

public:
	void setViewportAlpha(unsigned alpha) noexcept;

	/**
	 * @brief Sets various parameters to shape, scale, and pad the system's Viewport. Thread-safe.
	 * @param[in] W :: The width of the output texture in pixels.
	 * @param[in] H :: The height of the output texture in pixels.
	 * @param[in] mult :: Integer multiplier of the texture size to adjust minimum size. Capped at 16.
	 * @param[in] pad :: The thickness of the padding (colorable, see 'setOutlineColor')
	 * surrounding the Viewport in pixels. When negative, doubles as a flag that controls whether
	 * to draw scanlines over the Viewport. Capped at -16..16.
	 * @return Boolean if successful.
	 */
	void setViewportSizes(int W, int H, int mult = 0, int ppad = 0) noexcept;
	auto getViewportSizes() const noexcept -> Viewport;

public:
	void resetMainWindow() noexcept;
	void setMainWindowTitle(const std::string& title, const std::string& desc);
	bool isMainWindowID(unsigned id) const noexcept;
	void raiseMainWindow() noexcept;

	bool renderPresent(bool core, const char* overlay_data) noexcept;
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
