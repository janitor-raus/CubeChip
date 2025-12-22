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
