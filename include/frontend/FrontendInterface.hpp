/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <map>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <utility>
#include <string>
#include <memory>
#include <functional>

/*==================================================================*/

/**
 * @brief Utility class for handling ImGui labels with optional tags.
 *
 * ImGui uses '##' as a separator between the visible label and an internal
 * identifier tag. This class encapsulates that functionality, allowing easy
 * manipulation of both the label and tag components.
 */
struct ImLabel {
	static constexpr const char* separator = "##";

	std::string value;

	ImLabel() noexcept = default;
	ImLabel(std::string v)      noexcept : value(std::move(v)) {}
	ImLabel(std::string_view v) noexcept : value(v) {}
	ImLabel(const char* v)      noexcept : value(v) {}

	/*==================================================================*/

	operator std::string_view() const noexcept { return value; }
	operator const char*() const noexcept { return value.c_str(); }

	/***/ std::string* operator->()       noexcept { return &value; }
	const std::string* operator->() const noexcept { return &value; }

	bool operator==(const ImLabel& other) const noexcept {
		return value == other.value;
	}

	/*==================================================================*/

	bool has_id() const noexcept {
		return value.find(separator) != std::string::npos;
	}

	std::string get_name() const noexcept {
		const auto pos = value.find(separator);
		return (pos == std::string::npos) ? value
			: std::string(value.data(), pos);
	}

	std::string get_id() const noexcept {
		const auto pos = value.find(separator);
		return (pos == std::string::npos) ? std::string()
			: std::string(value.data() + pos);
	}

	std::string get_id_or_label() const noexcept {
		const auto pos = value.find(separator);
		return (pos == std::string::npos) ? value
			: std::string(value.data() + pos);
	}

	/*==================================================================*/

	void set_name(std::string_view new_name) noexcept {
		if (has_id()) {
			value = std::string(new_name) + separator + get_id();
		} else {
			value = std::string(new_name);
		}
	}

	void set_id(std::string_view new_id) noexcept {
		if (new_id.empty()) {
			value = get_name();
		} else {
			value = get_name() + separator + std::string(new_id);
		}
	}

	void set_label(std::string_view new_name, std::string_view new_id) noexcept {
		if (new_id.empty()) {
			value = std::string(new_name);
		} else {
			value = std::string(new_name) + separator + std::string(new_id);
		}
	}
};

namespace std {
	template<>
	struct hash<ImLabel> {
		std::size_t operator()(const ImLabel& label) const noexcept {
			return std::hash<std::string>{}(label.value);
		}
	};
}

/*==================================================================*/

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

template <typename Fn>
concept VoidInvocable = std::is_nothrow_invocable_r_v<void, Fn>;

/*==================================================================*/

class FrontendInterface {
public:
	using Func = std::function<void()>;
	using Hook = std::shared_ptr<Func>;
	using LabelKey = ImLabel;
	using OrderKey = std::pair<std::size_t, std::string>;

private:
	struct HookRegistry {
		using HookBuffer = std::vector<std::weak_ptr<Func>>;

		HookBuffer buffer{};
		unsigned   offset{}; // used when merging, don't touch

		bool has_focus{}; // indicates focus status, don't touch
		bool first_hit{}; // set for a single frame only on focus

		// make Clang happy I guess.
		HookRegistry() = default;
	};

	using HookRegistryMapped = std::unordered_map
		<LabelKey, std::map<OrderKey, HookRegistry>>;

	template <typename T>
	struct RegistryBox {
		T registry, overflow;

		std::mutex registry_lock;
		std::mutex overflow_lock;
	};

	struct RegistryAggregate {
		RegistryBox<HookRegistry>     windows{};
		RegistryBox<HookRegistryMapped> menus{};
	};

	static inline std::unique_ptr
		<RegistryAggregate> s_hooks = nullptr;

public:
	/**
	 * @brief Registers a nothrow function to be called during the window rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 * Nesting is allowed, but unless lifetime persists, the hooks will be erased
	 * during the next loop of invocation during the same render frame.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_window(Fn&& fn) noexcept {
		auto shared_ptr = std::make_shared<Func>(std::forward<Fn>(fn));

		if (s_hooks->windows.registry_lock.try_lock()) { // may fail spuriously (fine)
			s_hooks->windows.registry.buffer.push_back(shared_ptr);
			s_hooks->windows.registry_lock.unlock();
		} else {
			std::unique_lock lock(s_hooks->windows.overflow_lock); // must wait to acquire
			s_hooks->windows.overflow.buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

	/**
	 * @brief Registers a nothrow function to be called during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_menu(const LabelKey& window_tag, const OrderKey& menu_title, Fn&& fn) noexcept {
		auto shared_ptr = std::make_shared<Func>(std::forward<Fn>(fn));

		if (s_hooks->menus.registry_lock.try_lock()) { // may fail spuriously (fine)
			s_hooks->menus.registry[window_tag.get_id_or_label()] \
				[menu_title].buffer.push_back(shared_ptr);
			s_hooks->menus.registry_lock.unlock();
		} else {
			std::unique_lock lock(s_hooks->menus.overflow_lock); // must wait to acquire
			s_hooks->menus.overflow[window_tag.get_id_or_label()] \
				[menu_title].buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

private:
	static inline HookRegistry* s_active_menu{};
public:
	static bool was_menu_clicked() noexcept {
		return s_active_menu && s_active_menu->first_hit;
	}

private:
	static bool merge_overflowing_windows() noexcept;
	static void invoke_registered_windows() noexcept;

	static bool merge_overflowing_menus(const LabelKey& tag) noexcept;
	static void invoke_registered_menus(const LabelKey& tag) noexcept;

public:
	static void init_context(const char* home_dir);
	static void quit_context();

	static void init_video(SDL_Window*, SDL_Renderer*);
	static void quit_video();

public:
	static SDL_Renderer* get_current_renderer()  noexcept;
	static unsigned      get_main_dockspace_id() noexcept;

public:
	static void  set_ui_zoom_scaling(float scale) noexcept;
	static float get_ui_zoom_scaling() noexcept;
	static void  set_ui_text_scaling(float scale) noexcept;
	static float get_ui_text_scaling() noexcept;

public:
	static void process_event(void* event);
	static void begin_new_frame();
	static void render_frame(SDL_Renderer* = nullptr);

	static void dock_next_window_to(unsigned id, bool first_time = false) noexcept;

	static void call_menubar(const char* window_name) noexcept;
	static void call_autohide_menubar(const char* window_name, bool& hidden) noexcept;
};
