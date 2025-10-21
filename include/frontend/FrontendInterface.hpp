/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <utility>
#include <string>
#include <functional>

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
	using HookRegistry = std::vector<std::weak_ptr<Func>>;
	using HookRegistryMap = std::unordered_map<std::string, HookRegistry>;

	static inline std::shared_mutex
		sRegistryAccessMutex;

	static inline HookRegistry
		sHookRegistry_Windows;

	static inline HookRegistryMap
		sHookRegistry_Menu;

private:
	static void invokeRegisteredFuncs(HookRegistry& registry) noexcept;
	static void invokeRegisteredFuncs(HookRegistry& registry, const std::string& key) noexcept;

public:
	/**
	 * Registers a function to be called during the window rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook registerWindow(Fn&& fn) noexcept {
		auto shared_ptr{ std::make_shared<Func>(std::forward<Fn>(fn)) };
		{
			std::unique_lock lock{ sRegistryAccessMutex };
			sHookRegistry_Windows.push_back(shared_ptr);
		}
		return shared_ptr;
	}

	/**
	 * Registers a function to be called during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook registerMenu(const std::string& key, Fn&& fn) noexcept {
		auto shared_ptr{ std::make_shared<Func>(std::forward<Fn>(fn)) };
		{
			std::unique_lock lock{ sRegistryAccessMutex };
			sHookRegistry_Menu[key].push_back(shared_ptr);
		}
		return shared_ptr;
	}

private:
	static void invokeRegisteredWindows() noexcept {
		invokeRegisteredFuncs(sHookRegistry_Windows);
	}

	static void invokeRegisteredMenus() noexcept {
		for (auto& [key, registry] : sHookRegistry_Menu) {
			if (!registry.empty()) { invokeRegisteredFuncs(registry, key); }
		}
	}

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
