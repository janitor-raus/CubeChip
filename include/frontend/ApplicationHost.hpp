/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "SimpleMRU.hpp"
#include "FileItem.hpp"

/*==================================================================*/

#ifndef PROJECT_VERSION_MAJOR_I
	#define PROJECT_VERSION_MAJOR_I 0
#endif
#ifndef PROJECT_VERSION_MINOR_I
	#define PROJECT_VERSION_MINOR_I 0
#endif
#ifndef PROJECT_VERSION_PATCH_I
	#define PROJECT_VERSION_PATCH_I 0
#endif
#ifndef PROJECT_VERSION_TWEAK_I
	#define PROJECT_VERSION_TWEAK_I 0
#endif
#ifndef PROJECT_VERSION_MAJOR
	#define PROJECT_VERSION_MAJOR "0"
#endif
#ifndef PROJECT_VERSION_MINOR
	#define PROJECT_VERSION_MINOR "0"
#endif
#ifndef PROJECT_VERSION_PATCH
	#define PROJECT_VERSION_PATCH "0"
#endif
#ifndef PROJECT_VERSION_TWEAK
	#define PROJECT_VERSION_TWEAK "0"
#endif
#ifndef PROJECT_VERSION_GHASH
	#define PROJECT_VERSION_GHASH "unknown"
#endif
#ifndef PROJECT_VERSION_WITH_DATE
	#define PROJECT_VERSION_WITH_DATE "0.0.0.0"
#endif
#ifndef PROJECT_VERSION_WITH_HASH
	#define PROJECT_VERSION_WITH_HASH "0.0.0.0.unknown"
#endif
#ifndef PROJECT_NAME
	#define PROJECT_NAME "CubeChip?"
#endif

/*==================================================================*/

struct ProjectVersion {
	inline static constexpr auto major_i = PROJECT_VERSION_MAJOR_I;
	inline static constexpr auto minor_i = PROJECT_VERSION_MINOR_I;
	inline static constexpr auto patch_i = PROJECT_VERSION_PATCH_I;
	inline static constexpr auto tweak_i = PROJECT_VERSION_TWEAK_I;

	inline static constexpr const char* major = PROJECT_VERSION_MAJOR;
	inline static constexpr const char* minor = PROJECT_VERSION_MINOR;
	inline static constexpr const char* patch = PROJECT_VERSION_PATCH;
	inline static constexpr const char* tweak = PROJECT_VERSION_TWEAK;

	inline static constexpr const char* ghash = PROJECT_VERSION_GHASH;

	inline static constexpr const char* with_date = PROJECT_VERSION_WITH_DATE;
	inline static constexpr const char* with_hash = PROJECT_VERSION_WITH_HASH;
};

inline static constexpr ProjectVersion c_app_ver{};

constexpr const char* c_app_name = PROJECT_NAME;
constexpr const char* c_org_name = "";

/*==================================================================*/

class ISystemEmu;
union SDL_Event;

/*==================================================================*/

class ApplicationHost final {
	ApplicationHost() noexcept;

	ApplicationHost(const ApplicationHost&) = delete;
	ApplicationHost& operator=(const ApplicationHost&) = delete;

	static void set_open_file_dialog_result(std::string_view file) noexcept;

	static constexpr std::size_t s_mru_limit = 10;
	inline static SimpleMRU<FileItem> s_file_mru = s_mru_limit;

	static void import_mru(std::string* src) noexcept {
		for (std::size_t i = 0; i < s_mru_limit; ++i) {
			auto& entry = src[s_mru_limit - 1 - i];
			if (entry.empty()) { continue; }
			s_file_mru.insert(std::move(entry));
		}
	}

	static void export_mru(std::string* dst) noexcept {
		for (std::size_t i = 0; i < s_file_mru.size(); ++i) {
			dst[i] = s_file_mru[i]->string();
		}
	}

	struct Settings {
		float ui_zoom_scale = 1.0f;
		float ui_text_scale = 1.0f;
		bool  borderless_view_mode = false;
		std::string file_mru_cache[s_mru_limit];
	};
	static Settings s_settings;

/*==================================================================*/

private:
	class SystemInstance final {
		struct StopSystemThread {
			void operator()(ISystemEmu*) noexcept;
		};
		using SystemCore = std::unique_ptr
			<ISystemEmu, StopSystemThread>;

	public:
		SystemCore core;

		/***/ auto* operator->()       noexcept { return core.get(); }
		const auto* operator->() const noexcept { return core.get(); }

		operator bool() const noexcept { return bool(core); }
	};

	using SystemID = std::size_t;
	using SystemMap = std::unordered_map<SystemID, SystemInstance>;

	SystemMap m_systems{};

	SimpleMRU<SystemID> m_focus_mru;

	void prune_terminated_systems() noexcept;
	void find_last_focused_system() noexcept;

	void unload_system_instance(SystemID system_id = 0) noexcept;
	void insert_system_instance(ISystemEmu* system) noexcept;

	static inline std::string s_config_path{};

/*==================================================================*/

private:
	void load_file_from_disk(std::string_view file_path) noexcept;
	void handle_main_hotkeys() noexcept;
	void setup_gui_callables() noexcept;

/*==================================================================*/

public:
	static ApplicationHost* init_application(
		std::string_view config_name,
		std::string_view game_file_path,
		bool headless = false
	) noexcept;

	void quit_application() noexcept;

	int handle_client_events(const SDL_Event& event) noexcept;
	int process_client_frame();
};
