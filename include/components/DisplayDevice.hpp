/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <functional>

#include "AtomicBox.hpp"
#include "FramePacket.hpp"
#include "TripleBuffer.hpp"

/*==================================================================*/

class DisplayDevice {
	using ScopedRead = AtomicBox<FramePacket::Metadata>::ScopedRead;
	using ScopedEdit = AtomicBox<FramePacket::Metadata>::ScopedEdit;

	struct DisplayContext;
	std::unique_ptr<DisplayContext> m_context;

public:
	using Callable  = std::function<void()>;
	using Swapchain = TripleBuffer<FramePacket>;

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
	auto read_metadata() noexcept -> ScopedRead;
	auto edit_metadata() noexcept -> ScopedEdit;

public:
	DisplayDevice(std::size_t W, std::size_t H, void* sdl_renderer) noexcept;

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

	void set_osd_callable(Callable callable) noexcept;
	void render_display() noexcept;
	void render_settings_menu() noexcept;
};
