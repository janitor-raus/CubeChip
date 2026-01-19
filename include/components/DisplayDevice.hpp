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
#include "FrontendInterface.hpp"

/*==================================================================*/

class DisplayDevice {
	using Callable  = FrontendInterface::Func;
	using Swapchain = TripleBuffer<FramePacket>;

	struct DisplayContext; // make *real* clang happy
	friend struct DisplayContext;

	std::unique_ptr<DisplayContext> m_context;

public:
	/**
	 * @brief TripleBuffer swapchain with a fixed-size buffer of T and
	 * an instance of metadata to propagate both data and state. Refer
	 * to the 'acquire()' and 'present()' member methods for use info.
	 */
	auto swapchain()       noexcept ->       Swapchain&;
	auto swapchain() const noexcept -> const Swapchain&;

public:
	/**
	 * @brief Staging area for metadata, serving both as persistent
	 * storage, as well as for writing to the swapchain's internal
	 * metadata instance to propagate state changes atomically.
	**/
	FramePacket::Metadata
		metadata_staging;

private:
	DisplayDevice(std::size_t W, std::size_t H, const char* name, std::size_t bpp, int) noexcept;

public:
	DisplayDevice(std::size_t W, std::size_t H, const char* name = nullptr, std::size_t bpp = 4) noexcept;

	~DisplayDevice() noexcept;
	DisplayDevice(DisplayDevice&&) noexcept;
	DisplayDevice& operator=(DisplayDevice&&) noexcept;

public:
	int  get_screen_rotation() const noexcept;
	void set_screen_rotation(int rotation) noexcept;

	bool get_integer_scaling() const noexcept;
	void set_integer_scaling(bool enable) noexcept;

	bool get_utilize_shaders() const noexcept;
	void set_utilize_shaders(bool enable) noexcept;

	auto get_window_label() const noexcept -> std::string;
	void set_window_label(std::string_view name) noexcept;

	void set_shutdown_signal(bool* signal) noexcept;
	void set_osd_callable(Callable callable) noexcept;

	#if !defined(NDEBUG) || defined(DEBUG)
private:
	void bind_debug_menu_hooks() noexcept;
	void free_debug_menu_hooks() noexcept;

	FrontendInterface::Hook m_debug_border_width;
	FrontendInterface::Hook m_debug_inner_margin;

	FrontendInterface::Hook m_debug_border_color;
	FrontendInterface::Hook m_debug_texture_tint;

	FrontendInterface::Hook m_debug_texture_zoom;
	FrontendInterface::Hook m_debug_pixel_ratio;

	FrontendInterface::Hook m_debug_linear_scaling;
	FrontendInterface::Hook m_debug_screen_rotation;

	FrontendInterface::Hook m_debug_shaders_enabled;
	FrontendInterface::Hook m_debug_screen_enabled;

	FrontendInterface::Hook m_reserved[2]{};
	#endif
};

/*==================================================================*/

namespace osd {
	void simple_stat_overlay(const std::string& overlay_data) noexcept;
}
