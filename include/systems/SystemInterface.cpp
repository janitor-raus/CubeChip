/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "ThreadAffinity.hpp"
#include "FrameLimiter.hpp"
#include "BasicLogger.hpp"

#include "SystemInterface.hpp"

/*==================================================================*/

void SystemInterface::start_workers() noexcept {
	if (!m_system_thread.joinable()) {
		initialize_system();
		m_system_thread = Thread([&](StopToken token) noexcept {
			SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
			thread_affinity::set_affinity(~0b11ull);

			do {
				await_next_frame(false);

				m_timer.start();
				main_system_loop();

				m_elapsed_frames += 1;
				m_benched_frames = has_system_state(EmuState::BENCH)
					? m_benched_frames + 1 : 0;
			} while (!token.stop_requested());
		});
	}
	if (!m_timing_thread.joinable()) {
		m_timing_thread = Thread([&](StopToken token) noexcept {
			SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
			thread_affinity::set_affinity(0b11ull);

			FrameLimiter Pacer(get_real_system_framerate());

			do {
				if (Pacer.isFrameReady(!has_system_state(EmuState::BENCH))) {
					Pacer.setLimiterProperties(get_real_system_framerate());

					if (!can_system_work()) [[unlikely]] { continue; }

					set_frame_stop_flag(true);
					notify_next_frame(true);
				}
			} while (!token.stop_requested());
		});
	}
}

void SystemInterface::stop_workers() noexcept {
	if (m_system_thread.joinable()) {
		m_system_thread.request_stop();
		notify_next_frame(true);
		m_system_thread.join();
	}
	if (m_timing_thread.joinable()) {
		m_timing_thread.request_stop();
		m_timing_thread.join();
	}
}

SystemInterface::SystemInterface() noexcept
	: m_statistics_data(std::make_shared<std::string>())
{
	static thread_local auto sRNG   = std::make_unique<Well512>();
	static thread_local auto sInput = std::make_unique<BasicKeyboard>();
	RNG   = sRNG.get();
	Input = sInput.get();

	m_statistics_work_buffer.reserve(1_KiB);
}

/*==================================================================*/

f32 SystemInterface::get_base_system_framerate() const noexcept
	{ return m_base_system_framerate.load(mo::relaxed); }

f32 SystemInterface::get_framerate_multiplier() const noexcept
	{ return m_framerate_multiplier.load(mo::relaxed); }

f32 SystemInterface::get_real_system_framerate() const noexcept
	{ return get_base_system_framerate() * get_framerate_multiplier(); }

void SystemInterface::set_base_system_framerate(f32 value) noexcept
	{ m_base_system_framerate.store(std::clamp(value, 24.0f, 100.0f), mo::relaxed); }

void SystemInterface::set_framerate_multiplier(f32 value) noexcept
	{ m_framerate_multiplier.store(std::clamp(value, 0.10f, 10.00f), mo::relaxed); }

void SystemInterface::append_statistics_data() noexcept {
	const auto framerate = get_real_system_framerate();
	const auto frametime = m_timer.get_elapsed_millis();
	const auto framespan = 1000.0f / framerate;

	format_statistics_data(
		"Framerate:{:9.3f} fps |{:9.3f}ms\n"
		"Frametime:{:9.3f} ms ({:>6.2f}%)\n",
		framerate, framespan, frametime,
		frametime / framespan * 100.0f);
}

void SystemInterface::create_statistics_data() noexcept {
	if (!has_system_state(EmuState::STATS)) { return; }
	append_statistics_data();

	m_statistics_data.store(std::make_shared<std::string> \
		(std::move(m_statistics_work_buffer)), mo::release);
	m_statistics_work_buffer.clear();
}

std::string SystemInterface::copy_statistics_string() const noexcept {
	return *m_statistics_data.load(mo::acquire);
}
