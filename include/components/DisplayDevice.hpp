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

class DisplayDevice {
	using Callable  = FrontendInterface::Func;
	using Swapchain = TripleBuffer<FramePacket>;

	struct DisplayContext {
		AtomSharedPtr<std::string> m_display_name;
		AtomSharedPtr<Callable>    m_osd_callable;
		FrontendInterface::Hook    m_render_hook;
		SDL_Holder<SDL_Texture>    m_sdl_texture;

		std::atomic<int>  m_screen_rotation{};
		std::atomic<bool> m_integer_scaling{};
		std::atomic<bool> m_utilize_shaders{};

		DisplayContext(
			std::size_t W, std::size_t H,
			const char* name, std::size_t bpp
		) noexcept;

		/**
		 * @brief TripleBuffer swapchain with a fixed-size buffer of T and
		 * an instance of metadata to propagate both data and state. Refer
		 * to the 'acquire()' and 'present()' member methods for use info.
		 */
		Swapchain m_swapchain;

	private:
		void render_display_window() noexcept;
	};

	std::unique_ptr<DisplayContext> m_context;

public:
	auto swapchain()       noexcept ->       Swapchain& { return m_context->m_swapchain; }
	auto swapchain() const noexcept -> const Swapchain& { return m_context->m_swapchain; }

public:
	/**
	 * @brief Staging area for metadata, serving both as persistent
	 * storage, as well as for writing to the swapchain's internal
	 * metadata instance to propagate state changes atomically.
	**/
	FrameMetadata metadata_staging;

public:
	DisplayDevice(
		std::size_t W, std::size_t H,
		const char* name = nullptr, std::size_t bpp = 4
	) noexcept;

private:
	DisplayDevice(
		std::size_t W, std::size_t H,
		const char* name, std::size_t bpp, int
	) noexcept;

public:
	void set_screen_rotation(int rotation) noexcept;
	void set_integer_scaling(bool enable)  noexcept;
	void set_utilize_shaders(bool enable)  noexcept;

	void set_display_name(std::string_view name) noexcept;
	void set_osd_callable(Callable&& callable)   noexcept;
};

/*==================================================================*/

namespace osd {
	void simple_stat_overlay(const std::string& overlay_data) noexcept;
}

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

