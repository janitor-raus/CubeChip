/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <unordered_map>
#include <atomic>
#include <shared_mutex>
#include <vector>
#include <utility>
#include <string>
#include <functional>

#include "HDIS_HCIS.hpp"

/*==================================================================*/

struct ImVec2;

namespace ImGui {
	ImVec2 clamp(const ImVec2& value, const ImVec2& min, const ImVec2& max) noexcept;
	ImVec2 floor(const ImVec2& value) noexcept;
	ImVec2 ceil(const ImVec2& value) noexcept;
	ImVec2 abs(const ImVec2& value) noexcept;
	ImVec2 min(const ImVec2& value, const ImVec2& min) noexcept;
	ImVec2 max(const ImVec2& value, const ImVec2& max) noexcept;
}

/*==================================================================*/

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

template <typename Fn>
concept VoidInvocable = std::is_invocable_r_v<void, Fn>;


/*==================================================================*/

class FrontendInterface {
	using Func = std::function<void()>;
	using Hook = std::shared_ptr<Func>;
	using Key  = std::string;

	struct HookRegistry {
		using HookBuffer = std::vector<std::weak_ptr<Func>>;

		HookBuffer  buffer{};
		std::size_t offset{};
	};

	using HookRegistryMenuMap = std::unordered_map
		<Key, std::unordered_map<Key, HookRegistry>>;

	template <typename T>
	struct RegistryBox {
		T registry, overflow;

		alignas(HDIS) std::shared_mutex
			registry_lock, overflow_lock;
	};

	static inline RegistryBox<HookRegistry>
		sHooks_Windows;

	static inline RegistryBox<HookRegistryMenuMap>
		sHooks_Menus;

public:
	/**
	 * Registers a function to be called during the window rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 * Nesting is allowed, but unless lifetime persists, the hooks will be erased
	 * during the next loop of invocation during the same render frame.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook registerWindow(Fn&& fn) noexcept {
		auto shared_ptr{ std::make_shared<Func>(std::forward<Fn>(fn)) };

		if (sHooks_Windows.registry_lock.try_lock()) { // may fail spuriously (fine)
			sHooks_Windows.registry.buffer.push_back(shared_ptr);
			sHooks_Windows.registry_lock.unlock();
		} else {
			std::unique_lock lock{ sHooks_Windows.overflow_lock }; // must wait to acquire
			sHooks_Windows.overflow.buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

	/**
	 * Registers a function to be called during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook registerMenu(const Key& window_tag, const Key& menu_title, Fn&& fn) noexcept {
		auto shared_ptr{ std::make_shared<Func>(std::forward<Fn>(fn)) };

		if (sHooks_Menus.registry_lock.try_lock()) { // may fail spuriously (fine)
			sHooks_Menus.registry[window_tag][menu_title].buffer.push_back(shared_ptr);
			sHooks_Menus.registry_lock.unlock();
		} else {
			std::unique_lock lock{ sHooks_Menus.overflow_lock }; // must wait to acquire
			sHooks_Menus.overflow[window_tag][menu_title].buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

private:
	static bool mergeOverflowingWindows() noexcept;
	static void invokeRegisteredWindows() noexcept;

	static bool mergeOverflowingMenus(const Key& tag) noexcept;
	static void invokeRegisteredMenus(const Key& tag) noexcept;

public:
	static void InitializeContext(const char*);
	static void InitializeVideo(SDL_Window*, SDL_Renderer*);
	static void Shutdown();

	static void ProcessEvent(void* event);
	static void NewFrame();
	static void RenderFrame(SDL_Renderer*);

	static void UpdateFontScale(const void* data, int size, float scale);
	template <typename T, std::size_t N>
	static void UpdateFontScale(T(&appFont)[N], float scale)
		{ UpdateFontScale(appFont, N, scale); }

	static void PrepareViewport(
		bool enable, bool integer_scaling,
		int width, int height, int rotation,
		const char* overlay, SDL_Texture* texture
	);

private:
	static void initializeMainDockspace();
};
