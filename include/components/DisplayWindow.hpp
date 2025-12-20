/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <string_view>

#include "FramePacket.hpp"
#include "TripleBuffer.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "FrontendInterface.hpp"

/*==================================================================*/

class DisplayWindow {
	using Callable = FrontendInterface::Func;

	AtomSharedPtr<std::string> mWindowName;
	AtomSharedPtr<Callable>    mCallable;
	FrontendInterface::Hook    mRenderHook;
	SDL_Holder<SDL_Texture>    mTexture;

	std::atomic<int>  mScreenRotation{};
	std::atomic<bool> mIntegerScaling{};
	std::atomic<bool> mCallShaderPass{};

public:
	/**
	 * @brief TripleBuffer swapchain with a fixed-size buffer of T and
	 * an instance of metadata to propagate both data and state. Refer
	 * to the 'acquire()' and 'present()' member methods for use info.
	 */
	TripleBufferX<FramePacket> swapchain;

	/**
	 * @brief Staging area for metadata, serving both as persistent
	 * storage, as well as for writing to the swapchain's internal
	 * metadata instance to propagate state changes atomically.
	**/
	FrameMetadata metadata_staging;


public:
	DisplayWindow(std::size_t W, std::size_t H, const char* name) noexcept;

public:
	template <std::size_t W, std::size_t H>
	[[nodiscard]] static
	DisplayWindow Create(const char* name) noexcept {
		static_assert(W > 0 && H > 0, "W and H must be greater than zero.");
		static_assert(W <= 8192 && H <= 8192, "W and H are capped to 8192.");

		return DisplayWindow(W, H, name);
	}

	[[nodiscard]] static
	DisplayWindow Create(std::size_t W, std::size_t H, const char* name) noexcept;

public:
	void SetScreenRotation(int rotation) noexcept;
	void SetIntegerScaling(bool enable) noexcept;
	void SetCallShaderPass(bool enable) noexcept;

	void SetWindowName(std::string_view name) noexcept;
	void SetOverlayCallable(Callable&& callable) noexcept;

private:
	void RenderDisplayWindow() noexcept;
};

/*==================================================================*/

void SimpleStatOverlay(const std::string& overlay_data) noexcept;

/*==================================================================*/

//struct OverlayConfig {
//	std::string id;
//	ImVec2 relativeOffset = { 0.0f, 0.0f }; // 0..1 relative to parent content area
//	ImVec2 padding = { 5,5 };       // padding around text
//	bool draggable = true;        // can be moved by dragging
//};
//
//inline void DrawRelativeOverlay(
//	const std::string& text,
//	const OverlayConfig& cfg,
//	const std::string& parentWindowName
//) {
//	using namespace ImGui;
//	BeginChild(nullptr);
//	ImVec2 parentSize = GetWindowRegionAvail();
//	ImVec2 parentPos = GetWindowPos() + GetWindowContentRegionMin();
//	EndChild(); // we just wanted the parent window bounds
//
//	// Compute absolute position of overlay from relative offset
//	ImVec2 overlayPos = parentPos + ImVec2(
//		cfg.relativeOffset.x * parentSize.x,
//		cfg.relativeOffset.y * parentSize.y
//	);
//
//	// Compute text size and body size
//	ImVec2 textSize = CalcTextSize(text.c_str());
//	ImVec2 bodySize = textSize + cfg.padding * 2.0f;
//
//	// Draw overlay as a small child window for persistence & dragging
//	std::string childName = "##Overlay_" + cfg.id;
//	SetNextWindowPos(overlayPos, ImGuiCond_FirstUseEver);
//	ImGuiWindowFlags flags =
//		ImGuiWindowFlags_NoTitleBar |
//		ImGuiWindowFlags_AlwaysAutoResize |
//		ImGuiWindowFlags_NoSavedSettings;
//
//	if (!cfg.draggable)
//		flags |= ImGuiWindowFlags_NoMove;
//
//	if (Begin(childName.c_str(), nullptr, flags)) {
//		ImDrawList* dl = GetWindowDrawList();
//		ImVec2 winPos = GetWindowPos();
//		ImU32 bgColor = IM_COL32(0, 0, 0, 128);
//
//		// Rounded background
//		dl->AddRectFilled(winPos, winPos + bodySize, bgColor, 5.0f);
//
//		// Shadowed text
//		ImVec2 textPos = winPos + cfg.padding;
//		dl->AddText(textPos + ImVec2{ 1,1 }, IM_COL32(0, 0, 0, 255), text.c_str());
//		dl->AddText(textPos, IM_COL32(255, 255, 255, 255), text.c_str());
//
//		End();
//	}
//
//	// Clamp overlay to parent content area
//	ImVec2 winPos = GetWindowPos();
//	ImVec2 winSize = GetWindowSize();
//	ImVec2 maxPos = parentPos + parentSize - winSize;
//	ImVec2 newPos = winPos;
//	newPos.x = std::max(parentPos.x, std::min(winPos.x, maxPos.x));
//	newPos.y = std::max(parentPos.y, std::min(winPos.y, maxPos.y));
//	SetWindowPos(newPos);
//}

