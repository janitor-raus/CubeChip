/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <optional>
#include <utility>
#include <array>
#include <span>
#include <bit>

#include "EzMaths.hpp"
#include "AtomSharedPtr.hpp"
#include "HDIS_HCIS.hpp"
#include "Thread.hpp"

#include "WindowHost.hpp"
#include "BasicLogger.hpp"
#include "ArrayOps.hpp"
#include "SimpleTimer.hpp"
#include "BasicInput.hpp"
#include "Well512.hpp"

#include "FileImage.hpp"

#include <SDL3/SDL_scancode.h>

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

class alignas(HDIS) SystemInterface {

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
	std::unique_ptr<Well512>       m_rng;
	std::unique_ptr<BasicKeyboard> m_input;

protected:
	SystemInterface(std::string_view family_name) noexcept;

public:
	virtual ~SystemInterface() noexcept = default;

public:
	void start_workers() noexcept;
	void stop_workers() noexcept;

	virtual const SystemDescriptor& get_descriptor() const noexcept = 0;
	static std::string make_system_id(u32 id, std::string_view family_name) noexcept;

public:
	std::string get_system_id() const noexcept;

protected:
	std::atomic<f32> m_base_system_framerate{};
	std::atomic<f32> m_framerate_multiplier = 1.0f;

	u32 m_benched_frames{};
	u32 m_elapsed_frames{};

protected:
	bool m_is_window_visible = true;
	bool m_is_window_focused = true;

public:
	bool is_window_visible() const noexcept { return m_is_window_visible; }
	bool is_window_focused() const noexcept { return m_is_window_focused; }

	void is_window_visible(bool state) noexcept { m_is_window_visible = state; }
	void is_window_focused(bool state) noexcept { m_is_window_focused = state; }

protected:
	WindowHost m_window_host;

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



template <std::size_t N>
class MemoryBank : public SimpleContainerFacade<MemoryBank<N>, std::uint8_t> {
	static_assert(std::has_single_bit(N),
		"MemoryBank size must be a power of two!");

	std::array<std::uint8_t, N> mem{};

public:
	constexpr       auto& operator[](std::size_t index)       noexcept
		{ return mem[index & (N - 1)]; }
	constexpr const auto& operator[](std::size_t index) const noexcept
		{ return mem[index & (N - 1)]; }

	constexpr auto size() const noexcept { return N; }

	constexpr       auto* data()       noexcept { return mem.data(); }
	constexpr const auto* data() const noexcept { return mem.data(); }
};
