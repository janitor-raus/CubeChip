/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <optional>
#include <utility>
#include <span>

#include "EzMaths.hpp"
#include "AtomSharedPtr.hpp"
#include "Thread.hpp"

#include "MemoryEditor.hpp"
#include "WindowHost.hpp"
#include "Parameter.hpp"
#include "FrameLimiter.hpp"
#include "BasicInput.hpp"
#include "Well512.hpp"
#include "UserInterface.hpp"

#include "FileImage.hpp"

#include <SDL3/SDL_scancode.h>
#include <fmt/format.h>

/*==================================================================*/

enum EmuState : u8 {
	NORMAL = 0x00, // normal operation (default state)
	HIDDEN = 0x01, // window is hidden (unfocused or minimized)
	PAUSED = 0x02, // paused by hotkey (explicitly a user action)
	HALTED = 0x04, // normal stop path (system-specific, e.g. powered-off)
	FATAL  = 0x08, // fatal error path (system-specific, e.g. illegal ops)
	BENCH  = 0x10, // benchmarking mode (cpu de-limited)
	STATS  = 0x20, // collect/display OSD stats
	RESET  = 0x40, // in-progress system reset
	DEBUG  = 0x80, // debugger enabled (reserved for future use)

	NOT_RUNNING  = HIDDEN | PAUSED | HALTED | FATAL | RESET, // emulation cannot progress
	CANNOT_PAUSE = HIDDEN | HALTED | FATAL | RESET, // pause-trigger is not allowed
	ANY_PAUSE    = HIDDEN | PAUSED | RESET, // emulation is currently paused
	ANY_STOP     = HALTED | FATAL | RESET, // emulation is currently stopped
};

struct SimpleKeyMapping {
	u32          idx; // index value associated with entry
	SDL_Scancode key; // primary key mapping
	SDL_Scancode alt; // alternative key mapping
};
using SimpleKeyVec = std::vector<SimpleKeyMapping>;

struct SystemDescriptor;

/*==================================================================*/

class ISystemEmu {

	Thread m_system_thread;

	std::atomic<u8> m_system_state = EmuState::NORMAL;
	EmuState m_cached_system_state = EmuState::NORMAL;

public:
	// Adds a State to the System, returns previous value of State.
	auto add_system_state(EmuState state) noexcept { return m_system_state.fetch_or ( state, mo::acq_rel) & state; }
	// Removes a State from the System, returns previous value of State.
	auto sub_system_state(EmuState state) noexcept { return m_system_state.fetch_and(~state, mo::acq_rel) & state; }
	// Toggles a State in the System, returns previous value of State.
	auto xor_system_state(EmuState state) noexcept { return m_system_state.fetch_xor( state, mo::acq_rel) & state; }
	// Sets the total System State, return previous value of State.
	auto set_system_state(EmuState state) noexcept { return m_system_state.exchange(state, mo::acq_rel) & state; }
	// Fetches the current total System State.
	auto get_system_state()         const noexcept { return m_system_state.load(mo::acquire);  }

	// Tests if the given State is present in the System.
	bool has_system_state(EmuState state) const noexcept { return !!(get_system_state() & state); }
	// Tests if the System is allowed to run (temporarily or not).
	bool can_system_work()                const noexcept { return !(get_system_state() & EmuState::NOT_RUNNING); }
	// Tests if the System is allowed to (un)pause on demand.
	bool can_system_pause()               const noexcept { return !(get_system_state() & EmuState::CANNOT_PAUSE); }

	// Attempts to (un)pause the System if possible, returns paused State value.
	std::optional<bool> try_pause_system() noexcept {
		if (!can_system_pause()) { return std::nullopt; }
		return !xor_system_state(EmuState::PAUSED);
	}

	// Fetches the cached total System State for the frame.
	auto get_cached_system_state()               const noexcept { return m_cached_system_state;  }
	// Tests if the given State is present in the cache.
	bool has_cached_system_state(EmuState state) const noexcept { return !!(m_cached_system_state & state); }

	bool is_system_state_synced() const noexcept {
		return get_system_state() == m_cached_system_state;
	}

protected:
	bool m_is_viewport_visible = true;
	bool m_is_viewport_focused = true;

public:
	bool is_viewport_visible() const noexcept { return m_is_viewport_visible; }
	bool is_viewport_focused() const noexcept { return m_is_viewport_focused; }

	bool force_viewport_visible(bool state) noexcept {
		return std::exchange(m_is_viewport_visible, state);
	}
	bool force_viewport_focused(bool state) noexcept {
		return std::exchange(m_is_viewport_focused, state);
	}

private:
	u32 : 32; // reserved for future use

public:
	const u32 instance_id;

public:
	BoundedParam<60.0f, 24.0f, 100.0f> m_base_system_framerate;
	BoundedParam< 1.0f,  0.1f,  10.0f> m_framerate_multiplier;

protected:
	f32 m_cached_real_framerate = 0.0f;
public:
	f32 get_real_system_framerate() const noexcept {
		return m_cached_real_framerate;
	}

protected:
	u32 m_benched_frames = 0;
	u32 m_elapsed_frames = 0;

protected:
	FrameLimiter m_pacer{};

private:
	std::string m_statistics_work_buffer{};
	AtomSharedPtr<std::string>
		m_statistics_data{};

protected:
	std::unique_ptr<Well512> m_rng;
	BasicKeyboard m_input;

protected:
	ISystemEmu(std::string_view window_name) noexcept;

public:
	virtual ~ISystemEmu() noexcept = default;

private:
	void prepare_user_interface() noexcept;

public:
	void start_worker() noexcept;
	void stop_worker() noexcept;

private:
	virtual void reset_family_data() noexcept = 0;
	virtual void reset_system_data() noexcept = 0;

	void perform_instance_reset() noexcept;

public:
	void request_instance_reset() noexcept;

public:
	virtual const SystemDescriptor& get_descriptor() const noexcept = 0;
	static std::string make_system_id(u32 id, std::string_view identifier) noexcept;

public:
	std::string get_system_id() const noexcept;

protected:
	WindowHost m_workspace_host;

	std::vector<UserInterface::Hook>
		m_frontend_hooks;

protected:
	std::string m_file_sha1_hash{};
	bool calc_file_image_sha1() noexcept;

	FileImage m_file_image{};
	void copy_file_image_to(std::span<u8> dest, std::size_t offset) noexcept;

	std::vector<std::string> m_system_paths{};
	auto add_system_path(
		std::string_view dir_name,
		std::string_view family_name
	) noexcept -> const std::string*;

protected:
	SimpleKeyVec m_custom_binds;

protected:
	WindowHost   m_memview_window;
	MemoryEditor m_memory_editor;

protected:
	virtual void main_system_loop() = 0;
	virtual void initialize_family() noexcept = 0;
	virtual void initialize_system() noexcept = 0;

protected:
	template <typename... Args>
	void format_statistics_data(std::string&& message, Args&&... args) noexcept {
		try {
			fmt::vformat_to(std::back_inserter(m_statistics_work_buffer),
				std::move(message), fmt::make_format_args(args...));
		} catch (...) { /* ignore */ }
	}

	template <typename... Args>
	void format_statistics_data(const std::string& message, Args&&... args) noexcept {
		try {
			fmt::vformat_to(std::back_inserter(m_statistics_work_buffer),
				message, fmt::make_format_args(args...));
		} catch (...) { /* ignore */ }
	}

protected:
	virtual void append_statistics_data() noexcept;
	/*****/ void create_statistics_data() noexcept;
	std::string  copy_statistics_string() const noexcept;
};

/*==================================================================*/

namespace osd {
	void simple_text_overlay(const std::string& overlay_data) noexcept;
	void key_press_indicator(float phase) noexcept;

	inline void key_press_indicator(double phase) noexcept { key_press_indicator(float(phase)); }
}
