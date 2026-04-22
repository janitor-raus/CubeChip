/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "HomeDirManager.hpp"
#include "ThreadAffinity.hpp"
#include "FrameLimiter.hpp"
#include "StringJoin.hpp"
#include "SimpleFileIO.hpp"
#include "Millis.hpp"
#include "SHA1.hpp"

#include "BasicLogger.hpp"
#include "ISystemEmu.hpp"
#include "SystemDescriptor.hpp"
#include "SystemStaging.hpp"

/*==================================================================*/

ISystemEmu::ISystemEmu(std::string_view window_name) noexcept
	: instance_id([]() noexcept {
		static std::atomic<u32> instance_counter = 1u;
		return instance_counter.fetch_add(1, mo::relaxed);
	}())
	, m_statistics_data(std::make_shared<std::string>())
	, m_rng(std::make_unique<Well512>(Millis::initial()))
	, m_workspace_host({ window_name, make_system_id(instance_id, "system")})
	, m_file_image(std::move(SystemStaging::file_image))
	, m_memview_window({ "Memory Editor", make_system_id(instance_id, "mem_edit") })
{
	m_statistics_work_buffer.reserve(1_KiB);

	prepare_user_interface();
}

void ISystemEmu::start_workers() noexcept {
	if (!m_system_thread.joinable()) {
		initialize_system();
		m_system_thread = Thread([&](StopToken token) noexcept {
			SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
			thread_affinity::set_affinity(~0b11ull);
			ScopedLogSource s(get_system_id());

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

			FrameLimiter pacer(get_real_system_framerate());

			do {
				if (pacer.is_frame_ready(!has_system_state(EmuState::BENCH))) {
					pacer.set_limiter_props(get_real_system_framerate());

					if (!can_system_work()) [[unlikely]] { continue; }

					set_frame_stop_flag(true);
					notify_next_frame(true);
				}
			} while (!token.stop_requested());
		});
	}
}

void ISystemEmu::stop_workers() noexcept {
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

/*==================================================================*/

std::string ISystemEmu::make_system_id(u32 id, std::string_view identifier) noexcept {
	return ::join_with(".", std::to_string(id), identifier);
}

std::string ISystemEmu::get_system_id() const noexcept {
	return make_system_id(instance_id, get_descriptor().family_name);
}

/*==================================================================*/

void ISystemEmu::copy_file_image_to(std::span<u8> dest, std::size_t offset) noexcept {
	std::memcpy(dest.data() + offset, m_file_image.data(),
		std::min(m_file_image.size(), dest.size() - offset));
}

bool ISystemEmu::calc_file_image_sha1() noexcept {
	if (!SystemStaging::sha1_hash.empty()) {
		m_file_sha1_hash = SystemStaging::sha1_hash;
		return true;
	} else {
		if (m_file_image.valid()) {
			m_file_sha1_hash = SHA1::from_span(m_file_image);
			blog.info("SHA1: {}", m_file_sha1_hash);
			return true;
		} else {
			blog.error("Unable to compute SHA1, file '{}' is inaccessible!",
				m_file_image.path());
			return false;
		}
	}
}

auto ISystemEmu::add_system_path(
	std::string_view dir_name,
	std::string_view family_name
) noexcept -> const std::string* {
	if (dir_name.empty()) { return nullptr; }

	const auto new_dir_path = fs::Path(HomeDirManager \
		::get_instance()->get_home_path()) / family_name / dir_name;

	const auto it = std::find(m_system_paths.begin(),
		m_system_paths.end(), new_dir_path);

	if (it != m_system_paths.end()) { return &(*it); }

	if (const auto dir_created = fs::create_directories(new_dir_path)) {
		return &m_system_paths.emplace_back(new_dir_path.string());
	} else {
		blog.error("Unable to create directory '{}': {}",
			new_dir_path.string(), dir_created.error().message());
		return nullptr;
	}
}

/*==================================================================*/

f32 ISystemEmu::get_base_system_framerate() const noexcept
	{ return m_base_system_framerate.load(mo::relaxed); }

f32 ISystemEmu::get_framerate_multiplier() const noexcept
	{ return m_framerate_multiplier.load(mo::relaxed); }

f32 ISystemEmu::get_real_system_framerate() const noexcept
	{ return get_base_system_framerate() * get_framerate_multiplier(); }

void ISystemEmu::set_base_system_framerate(f32 value) noexcept
	{ m_base_system_framerate.store(std::clamp(value, 24.0f, 100.0f), mo::relaxed); }

void ISystemEmu::set_framerate_multiplier(f32 value) noexcept
	{ m_framerate_multiplier.store(std::clamp(value, 0.10f, 10.00f), mo::relaxed); }

/*==================================================================*/

void ISystemEmu::append_statistics_data() noexcept {
	const auto framerate = get_real_system_framerate();
	const auto frametime = m_timer.get_elapsed_millis();
	const auto framespan = 1000.0f / framerate;

	format_statistics_data(
		"Framerate:{:9.3f} fps |{:9.3f}ms\n"
		"Frametime:{:9.3f} ms ({:>6.2f}%)\n",
		framerate, framespan, frametime,
		frametime / framespan * 100.0f
	);
}

void ISystemEmu::create_statistics_data() noexcept {
	if (!has_system_state(EmuState::STATS)) { return; }
	append_statistics_data();

	m_statistics_data.store(std::make_shared<std::string>(
		std::move(m_statistics_work_buffer)), mo::release);
	m_statistics_work_buffer.clear();
}

std::string ISystemEmu::copy_statistics_string() const noexcept {
	return *m_statistics_data.load(mo::acquire);
}
