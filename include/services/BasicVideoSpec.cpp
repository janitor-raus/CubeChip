/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef _WIN32
	#define NOMINMAX
	#pragma warning(push)
	#pragma warning(disable : 5039)
	#include <dwmapi.h>
	#pragma comment(lib, "Dwmapi")
	#pragma warning(pop)
#endif

#include "FrontendInterface.hpp"
#include "BasicVideoSpec.hpp"
#include "ColorOps.hpp"

#include <vector>
#include <SDL3/SDL.h>

/*==================================================================*/

static auto to_FRect(const ez::Frame& rect) noexcept {
	return SDL_FRect{ 0.0f, 0.0f, f32(rect.w), f32(rect.h) };
}

static auto to_FRect(const BasicVideoSpec::Viewport& viewport) noexcept {
	return SDL_FRect{ f32(viewport.pxpad), f32(viewport.pxpad),
		f32(viewport.frame.w * viewport.multi), f32(viewport.frame.h * viewport.multi) };
}

static auto to_Frame(SDL_Texture* texture) noexcept {
	f32 w, h;
	SDL_GetTextureSize(texture, &w, &h);
	return ez::Frame(s32(w), s32(h));
}

/*==================================================================*/
	#pragma region BasicVideoSpec Singleton Class

BasicVideoSpec::BasicVideoSpec(const Settings& settings) noexcept {
	mSuccessful = SDL_InitSubSystem(SDL_INIT_VIDEO);
	if (!mSuccessful) {
		showErrorBox("Failed to init Video Subsystem!");
		return;
	}

	mViewportScaleMode = std::max(0, settings.viewport.filtering % 3);

	mIntegerScaling    = settings.viewport.int_scale;
	mUsingScanlines    = settings.viewport.scanlines;

	mSuccessful = mMainWindow = SDL_CreateWindow(nullptr, 0, 0, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
	if (!mSuccessful) {
		showErrorBox("Failed to create main window!");
		return;
	}
	#if defined(_WIN32) && !defined(OLD_WINDOWS_SDK)
	else {
		const auto windowHandle{ SDL_GetPointerProperty(
			SDL_GetWindowProperties(mMainWindow),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			nullptr
		) };

		if (windowHandle) {
			const auto cornerMode{ DWMWCP_DONOTROUND };
			DwmSetWindowAttribute(
				static_cast<HWND>(windowHandle),
				DWMWA_WINDOW_CORNER_PREFERENCE,
				&cornerMode, sizeof(cornerMode)
			);
		}
	}
	#endif

	ez::Rect deco{};

	if (auto dummy{ sdl::make_unique(SDL_CreateWindow(
		nullptr, 64, 64, SDL_WINDOW_UTILITY | SDL_WINDOW_HIDDEN
	)) }) {
		#ifndef __APPLE__
		constexpr auto away{ -(1 << 15) };
		SDL_SetWindowPosition(dummy, away, away);
		#endif
		SDL_ShowWindow(dummy);
		SDL_SyncWindow(dummy);
		SDL_RenderPresent(mMainRenderer);
		SDL_GetWindowBordersSize(dummy, &deco.x, &deco.y, &deco.w, &deco.h);
	}

	auto window{ settings.window };
	normalizeRectToDisplay(window, deco, settings.first_run);

	SDL_SetWindowPosition(mMainWindow, window.x, window.y);
	SDL_SetWindowSize(mMainWindow, window.w, window.h);

	mSuccessful = mMainRenderer = SDL_CreateRenderer(mMainWindow, nullptr);
	if (!mSuccessful) {
		showErrorBox("Failed to create Main renderer!");
		return;
	}

	FrontendInterface::Initialize(mMainWindow, mMainRenderer);

	resetMainWindow();
}

BasicVideoSpec::~BasicVideoSpec() noexcept {
	FrontendInterface::Shutdown();

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

auto BasicVideoSpec::exportSettings() const noexcept -> Settings {
	Settings out;

	if (SDL_GetWindowFlags(mMainWindow) & SDL_WINDOW_MAXIMIZED) {
		SDL_RestoreWindow(mMainWindow);
		SDL_SyncWindow(mMainWindow);
	}

	SDL_GetWindowPosition(mMainWindow, &out.window.x, &out.window.y);
	SDL_GetWindowSize(mMainWindow, &out.window.w, &out.window.h);
	out.viewport.filtering = mViewportScaleMode;
	out.viewport.int_scale = mIntegerScaling;
	out.viewport.scanlines = mUsingScanlines;
	out.first_run = false;

	return out;
}

void BasicVideoSpec::setMainWindowTitle(const Str& title, const Str& desc) {
	SDL_SetWindowTitle(mMainWindow, (desc.empty() ? title : title + " :: " + desc).c_str());
}

bool BasicVideoSpec::isMainWindowID(u32 id) const noexcept {
	return id == SDL_GetWindowID(mMainWindow);
}

void BasicVideoSpec::showErrorBox(const char* const title) noexcept {
	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR, title,
		SDL_GetError(), nullptr
	);
}

f32 BasicVideoSpec::getDisplayRefreshRate(SDL_DisplayID display) noexcept {
	if (const auto* mode{ SDL_GetCurrentDisplayMode(display) }) {
		if (mode->refresh_rate_denominator > 0) {
			return f32(mode->refresh_rate_numerator) /
				f32(mode->refresh_rate_denominator);
		} else {
			return (mode->refresh_rate > 0.0f)
				? mode->refresh_rate : 60.0f;
		}
	}
	return 60.0f;
}

void BasicVideoSpec::normalizeRectToDisplay(ez::Rect& rect, ez::Rect& deco, bool first_run) noexcept {
	auto numDisplays{  0 }; // count of displays SDL found
	auto bestDisplay{ -1 }; // index of display our window will use
	bool rectIntersectsDisplay{};
	
	// 1: fetch all eligible display IDs
	auto displays{ sdl::make_unique(SDL_GetDisplays(&numDisplays)) };
	if (!displays || numDisplays <= 0) [[unlikely]]
		{ rect = Settings::defaults; return; }

	// 2: fill vector with usable display bounds rects
	std::vector<ez::Rect> displayBounds;
	displayBounds.reserve(numDisplays);

	for (auto i{ 0 }; i < numDisplays; ++i) {
		if (displays[i] == SDL_GetPrimaryDisplay()) { bestDisplay = i; }
		SDL_Rect display;
		if (SDL_GetDisplayUsableBounds(displays[i], &display)) {
			ez::Rect bounds{ display.x, display.y, display.w, display.h };
			displayBounds.push_back(std::move(bounds));
		}
	}
	if (displayBounds.empty()) [[unlikely]]
		{ rect = Settings::defaults; return; }

	// 3: validate rect w/h, use fallbacks if needed
	rect.w = std::max(rect.w, Settings::defaults.w);
	rect.h = std::max(rect.h, Settings::defaults.h);

	if (!first_run) {
		// 4: find largest window/display overlap, if any
		auto bestOverlap{ u64(0) };
		for (auto i{ 0u }; i < displayBounds.size(); ++i) {
			const auto overlapArea{ ez::intersect(rect, displayBounds[i]).area() };
			if (overlapArea > bestOverlap) { bestOverlap = overlapArea; bestDisplay = i; }
		}
	
		// 5: fall back to searching for closest display
		if ((rectIntersectsDisplay = !!bestOverlap) == false) {
			auto bestDistance{ u64(-1) };
			const auto currentCenter{ rect.center() };

			for (auto i{ 0u }; i < displayBounds.size(); ++i) {
				const auto displayCenter{ displayBounds[i].center() };
				const auto distance{ ez::distance(currentCenter, displayCenter) };
				if (distance < bestDistance) { bestDistance = distance; bestDisplay = i; }
			}
		}
	}

	// 6: shrink window to best fit chosen display
	const auto& target{ displayBounds[bestDisplay] };

	const auto up{ deco.x }, lt{ deco.y };
	const auto dn{ deco.w }, rt{ deco.h };

	rect.w = std::min(rect.w, target.w - lt - rt);
	rect.h = std::min(rect.h, target.h - up - dn);

	if (!rectIntersectsDisplay) {
		// 7a: if we didn't overlap before, center to display
		rect.x = target.x + (target.w - lt - rt - rect.w) / 2 + lt;
		rect.y = target.y + (target.h - up - dn - rect.h) / 2 + up;
	} else {
		// 7b: otherwise, clamp origin to lie within display bounds
		rect.x = std::clamp(rect.x, target.x + lt, target.x + target.w - rt - rect.w);
		rect.y = std::clamp(rect.y, target.y + up, target.y + target.h - dn - rect.h);
	}
}

void BasicVideoSpec::raiseMainWindow() {
	SDL_RaiseWindow(mMainWindow);
}

void BasicVideoSpec::resetMainWindow() {
	SDL_ShowWindow(mMainWindow);
	//SDL_SyncWindow(mMainWindow);

	mCurViewport = { Settings::defaults.w, Settings::defaults.h, 1, 0 };
	mViewportRotation = 0;

	mSystemTexture.reset();
	mWindowTexture.reset();
}

void BasicVideoSpec::setViewportAlpha(u32 alpha) noexcept {
	mTextureAlpha.store(u8(alpha), mo::release);
}

void BasicVideoSpec::setViewportSizes(s32 W, s32 H, s32 mult, s32 ppad) noexcept {
	mNewViewport.store(Viewport::pack(W, H, mult, ppad), mo::release);
}

auto BasicVideoSpec::getViewportSizes() const noexcept -> BasicVideoSpec::Viewport {
	return Viewport::unpack(mNewViewport.load(mo::acquire));
}

void BasicVideoSpec::setViewportScaleMode(s32 mode) noexcept {
	switch (mode) {
		case SDL_SCALEMODE_NEAREST:
		case SDL_SCALEMODE_LINEAR:
			SDL_SetTextureScaleMode(mSystemTexture,
				static_cast<SDL_ScaleMode>(mode));
			mViewportScaleMode = mode;
			[[fallthrough]];
		default: return;
	}
}

void BasicVideoSpec::cycleViewportScaleMode() noexcept {
	switch (mViewportScaleMode) {
		case SDL_SCALEMODE_NEAREST:
			setViewportScaleMode(SDL_SCALEMODE_LINEAR);
			break;
		default:
			setViewportScaleMode(SDL_SCALEMODE_NEAREST);
			break;
	}
}

void BasicVideoSpec::setBorderColor(u32 color) noexcept {
	mOutlineColor.store(color, mo::release);
}

/*==================================================================*/

void BasicVideoSpec::prepareWindowTexture() {
	const auto outerRect{ mCurViewport.padded() };

	if (to_Frame(mWindowTexture) != outerRect) {
		mSuccessful = mWindowTexture = SDL_CreateTexture(
			mMainRenderer,
			SDL_PIXELFORMAT_RGBX8888,
			SDL_TEXTUREACCESS_TARGET,
			outerRect.w, outerRect.h
		);

		if (!mSuccessful) {
			showErrorBox("Failed to create Window texture!");
		} else {
			SDL_SetTextureScaleMode(mWindowTexture, SDL_SCALEMODE_NEAREST);
			SDL_SetRenderTarget(mMainRenderer, mWindowTexture);
			SDL_SetRenderDrawColor(mMainRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
			SDL_RenderClear(mMainRenderer);
		}
	}
}

void BasicVideoSpec::prepareSystemTexture() {
	if (!mWindowTexture) { return; }

	if (to_Frame(mSystemTexture) != mCurViewport.frame) {

		mSuccessful = mSystemTexture = SDL_CreateTexture(
			mMainRenderer,
			SDL_PIXELFORMAT_RGBX8888,
			SDL_TEXTUREACCESS_STREAMING,
			mCurViewport.frame.w, mCurViewport.frame.h
		);

		if (!mSuccessful) {
			showErrorBox("Failed to create System texture!");
		} else {
			SDL_SetTextureScaleMode(mSystemTexture, static_cast<SDL_ScaleMode>(mViewportScaleMode));
			SDL_SetTextureAlphaMod(mSystemTexture, mTextureAlpha.load(mo::acquire));
		}
	}
}

void BasicVideoSpec::renderViewport() {
	if (!mWindowTexture && !mSystemTexture)
		[[unlikely]] { return; }

	if (mWindowTexture) {
		SDL_SetRenderTarget(mMainRenderer, mWindowTexture);

		const RGBA Color{ mOutlineColor.load(mo::acquire) };
		SDL_SetRenderDrawColor(mMainRenderer, Color.R, Color.G, Color.B, SDL_ALPHA_OPAQUE);
		const auto outerFRect{ to_FRect(mCurViewport.padded()) };
		SDL_RenderFillRect(mMainRenderer, &outerFRect);
	}

	if (mSystemTexture) {
		SDL_SetRenderDrawColor(mMainRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		const auto innerFRect{ to_FRect(mCurViewport) };
		SDL_RenderFillRect(mMainRenderer, &innerFRect);

		{
			void* pixels{}; s32 pitch;

			SDL_LockTexture(mSystemTexture, nullptr, &pixels, &pitch);
			displayBuffer.read(static_cast<u32*>(pixels), mCurViewport.frame.area());
			SDL_UnlockTexture(mSystemTexture);
		}

		SDL_RenderTexture(mMainRenderer, mSystemTexture, nullptr, &innerFRect);

		if (mUsingScanlines && mCurViewport.pxpad >= 2) {
			SDL_SetRenderDrawBlendMode(mMainRenderer, SDL_BLENDMODE_BLEND);
			SDL_SetRenderDrawColor(mMainRenderer, 0, 0, 0, 0x20);

			const auto outerFRect{ to_FRect(mCurViewport.padded()) };
			const auto drawLimit{ static_cast<s32>(outerFRect.h) };
			for (auto y{ 0 }; y < drawLimit; y += mCurViewport.pxpad) {
				SDL_RenderLine(mMainRenderer,
					outerFRect.x, static_cast<f32>(y),
					outerFRect.w, static_cast<f32>(y));
			}
		}
	}

	SDL_SetRenderTarget(mMainRenderer, nullptr);
}

void BasicVideoSpec::renderPresent(bool core, const char* overlay_data) {
	mCurViewport = getViewportSizes();

	if (core) { prepareWindowTexture(); }
	if (core) { prepareSystemTexture(); }

	renderViewport();

	FrontendInterface::NewFrame();

	const auto outerRect{ mCurViewport.rotate_if(mViewportRotation & 1).padded() };
	const auto frameHeight{ s32(FrontendInterface::GetFrameHeight()) };

	SDL_SetWindowMinimumSize(mMainWindow, outerRect.w, outerRect.h + frameHeight);

	FrontendInterface::PrepareViewport(
		mSuccessful && mWindowTexture, mIntegerScaling,
		outerRect.w, outerRect.h, mViewportRotation,
		overlay_data, mWindowTexture
	);
	FrontendInterface::PrepareGeneralUI();
	FrontendInterface::RenderFrame(mMainRenderer);

	SDL_RenderPresent(mMainRenderer);
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
