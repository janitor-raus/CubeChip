/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "Typedefs.hpp"
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
		s32 multi{}, pxpad{};

		constexpr Viewport(s32 w = 0, s32 h = 0, s32 multi = 0, s32 pxpad = 0) noexcept
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

		static constexpr auto pack(s32 w, s32 h, s32 multi, s32 pxpad) noexcept {
			return ((u32(w)     & 0xFFFu) <<  0) |
				   ((u32(h)     & 0xFFFu) << 12) |
				   ((u32(multi) & 0xFu)   << 24) |
				   ((u32(pxpad) & 0xFu)   << 28);
		}

		static constexpr auto unpack(u32 packed) noexcept {
			return Viewport{
				s32((packed >>  0) & 0xFFFu),
				s32((packed >> 12) & 0xFFFu),
				s32((packed >> 24) & 0xFu),
				s32((packed >> 28) & 0xFu)
			};
		}
	};

private:
	SDL_Unique<SDL_Window>   mMainWindow{};
	SDL_Unique<SDL_Renderer> mMainRenderer{};
	SDL_Unique<SDL_Texture>  mWindowTexture{};
	SDL_Unique<SDL_Texture>  mSystemTexture{};

/*==================================================================*/

	Viewport  mCurViewport{};
	Atom<u32> mNewViewport{};

	Atom<u32> mOutlineColor{};
	Atom<u8>  mTextureAlpha{ 0xFF };

	bool mUsingScanlines{};
	bool mIntegerScaling{};

	s32 mViewportRotation{};
	s32 mViewportScaleMode{};

public:
	TripleBuffer<u32> displayBuffer;

	struct Settings {
		static constexpr ez::Rect
			defaults{ 0, 0, 640, 480 };

		ez::Rect window{ defaults };
		struct Viewport {
			s32  filtering{ 0 };
			bool int_scale{ true };
			bool scanlines{ true };
		} viewport;
		bool first_run{ true };

		auto map() noexcept {
			return SettingsMap{
				makeSetting("VIDEO.Window.X", &window.x),
				makeSetting("VIDEO.Window.Y", &window.y),
				makeSetting("VIDEO.Window.W", &window.w),
				makeSetting("VIDEO.Window.H", &window.h),
				makeSetting("VIDEO.Window.FirstRun", &first_run),
				makeSetting("VIDEO.Viewport.Filtering", &viewport.filtering),
				makeSetting("VIDEO.Viewport.Int_Scale", &viewport.int_scale),
				makeSetting("VIDEO.Viewport.Scanlines", &viewport.scanlines),
			};
		}
	};

	[[nodiscard]]
	auto exportSettings() const noexcept -> Settings;

private:
	BasicVideoSpec(const Settings& settings) noexcept;
	~BasicVideoSpec() noexcept;
	BasicVideoSpec(const BasicVideoSpec&) = delete;
	BasicVideoSpec& operator=(const BasicVideoSpec&) = delete;

/*==================================================================*/

	static inline bool mSuccessful{ true };

public:
	static auto* initialize(const Settings& settings) noexcept {
		static BasicVideoSpec self(settings);
		return mSuccessful ? &self : nullptr;
	}

	static bool isSuccessful() noexcept { return mSuccessful; }
	static void showErrorBox(const char* const title) noexcept;

/*==================================================================*/

public:
	f32  getDisplayRefreshRate(SDL_DisplayID display) noexcept;
	void normalizeRectToDisplay(ez::Rect& rect, ez::Rect& deco, bool first_run) noexcept;

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

	void rotateViewport(s32 delta) noexcept {
		mViewportRotation += delta;
		mViewportRotation &= 3;
	}
	void setViewportRotation(s32 value) noexcept {
		mViewportRotation = value & 3;
	}

	auto getViewportScaleMode() const noexcept { return mViewportScaleMode; }
	void setViewportScaleMode(s32 mode) noexcept;
	void cycleViewportScaleMode() noexcept;
	void setBorderColor(u32 color) noexcept;

/*==================================================================*/

private:
	void prepareWindowTexture();
	void prepareSystemTexture();
	void renderViewport();

public:
	void setViewportAlpha(u32 alpha) noexcept;

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
	void setViewportSizes(s32 W, s32 H, s32 mult = 0, s32 ppad = 0) noexcept;
	auto getViewportSizes() const noexcept -> Viewport;

public:
	void resetMainWindow();
	void setMainWindowTitle(const Str& title, const Str& desc);
	bool isMainWindowID(u32 id) const noexcept;
	void raiseMainWindow();

	void renderPresent(bool core, const char* overlay_data);
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
