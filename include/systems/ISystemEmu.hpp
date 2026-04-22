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
#include "SimpleTimer.hpp"
#include "BasicInput.hpp"
#include "Well512.hpp"
#include "UserInterface.hpp"

#include "FileImage.hpp"

#include <SDL3/SDL_scancode.h>
#include <fmt/format.h>

/*==================================================================*/

enum EmuState : u8 {
	NORMAL = 0x00, // normal operation
	HIDDEN = 0x01, // window is hidden
	PAUSED = 0x02, // paused by hotkey
	HALTED = 0x04, // normal end path
	FATAL  = 0x08, // fatal error path
	BENCH  = 0x10, // benchmarking mode
	STATS  = 0x20, // produce OSD stats
	DUMMY  = 0x40, // dummy mode (no audio/video)
	DEBUG  = 0x80, // debugger enabled

	NOT_RUNNING = HIDDEN | PAUSED | HALTED | FATAL, // when emulation must wait
	CANNOT_PAUSE = HIDDEN | HALTED | FATAL, // when pausing is disallowed
};

struct SimpleKeyMapping {
	u32          idx; // index value associated with entry
	SDL_Scancode key; // primary key mapping
	SDL_Scancode alt; // alternative key mapping
};

class HomeDirManager;
struct SystemDescriptor;

/*==================================================================*/

class /*alignas(HDIS)*/ ISystemEmu {

	Thread m_system_thread;
	Thread m_timing_thread;

	std::atomic<u8> m_system_state = EmuState::NORMAL;

	std::atomic<bool> m_next_frame_flag{};
	std::atomic<bool> m_stop_frame_flag{};

public:
	const u32 instance_id;

protected:
	SimpleTimer m_timer{};

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
	void start_workers() noexcept;
	void stop_workers() noexcept;

	virtual const SystemDescriptor& get_descriptor() const noexcept = 0;
	static std::string make_system_id(u32 id, std::string_view identifier) noexcept;

public:
	std::string get_system_id() const noexcept;

protected:
	std::atomic<f32> m_base_system_framerate{};
	std::atomic<f32> m_framerate_multiplier = 1.0f;

	u32 m_benched_frames{};
	u32 m_elapsed_frames{};

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

protected:
	WindowHost m_workspace_host;

	std::vector<UserInterface::Hook>
		m_frontend_hooks;

protected:
	FileImage m_file_image{};
	void copy_file_image_to(std::span<u8> dest, std::size_t offset) noexcept;

	std::string m_file_sha1_hash{};
	bool calc_file_image_sha1() noexcept;

	std::vector<std::string> m_system_paths{};
	auto add_system_path(
		std::string_view dir_name,
		std::string_view family_name
	) noexcept -> const std::string*;

protected:
	WindowHost   m_memview_window;
	MemoryEditor m_memory_editor;

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
	bool can_system_work()               const noexcept { return !(get_system_state() & EmuState::NOT_RUNNING); }
	// Tests if the System is allowed to (un)pause on demand.
	bool can_system_pause()              const noexcept { return !(get_system_state() & EmuState::CANNOT_PAUSE); }

	// Attempts to (un)pause the System if possible, returns paused State value.
	std::optional<bool> try_pause_system() noexcept {
		if (!can_system_pause()) { return std::nullopt; }
		return !xor_system_state(EmuState::PAUSED);
	}

protected:
	void set_base_system_framerate(f32 value) noexcept;
public:
	void set_framerate_multiplier(f32 value) noexcept;

public:
	f32  get_base_system_framerate() const noexcept;
	f32  get_framerate_multiplier()  const noexcept;
	f32  get_real_system_framerate() const noexcept;

protected:
	void set_frame_stop_flag(bool state) noexcept { m_stop_frame_flag.store(state, mo::relaxed); }
	auto get_frame_stop_flag()     const noexcept { return m_stop_frame_flag.load(mo::relaxed);  }

private:
	void notify_next_frame(bool state) noexcept {
		m_next_frame_flag.store(state, mo::release);
		m_next_frame_flag.notify_one();
	}
	void await_next_frame(bool state) noexcept {
		m_next_frame_flag.wait(state, mo::acquire);
		m_next_frame_flag.store(state, mo::release);
	}

protected:
	virtual void main_system_loop() = 0;
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
