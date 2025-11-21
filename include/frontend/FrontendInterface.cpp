/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <cmath>
#include <algorithm>
#include <string>

#include "FrontendInterface.hpp"
#include "HomeDirManager.hpp"
#include "BasicLogger.hpp"

#include "fonts/RobotoMono.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

/*==================================================================*/

static ImGuiID sMainDockID{};
static ImGuiID sTaskbarDockID{};
static float   sTaskbarSize{};

namespace ImGui {
	ImVec2 clamp(const ImVec2& value, const ImVec2& min, const ImVec2& max) noexcept {
		return { std::clamp(value.x, min.x, max.x), std::clamp(value.y, min.y, max.y) };
	}

	ImVec2 floor(const ImVec2& value) noexcept {
		return { std::floor(value.x), std::floor(value.y) };
	}

	ImVec2 ceil(const ImVec2& value) noexcept {
		return { std::ceil(value.x), std::ceil(value.y) };
	}

	ImVec2 abs(const ImVec2& value) noexcept {
		return { std::abs(value.x), std::abs(value.y) };
	}
	ImVec2 min(const ImVec2& value, const ImVec2& min) noexcept {
		return { std::min(value.x, min.x), std::min(value.y, min.y) };
	}
	ImVec2 max(const ImVec2& value, const ImVec2& max) noexcept {
		return { std::max(value.x, max.x), std::max(value.y, max.y) };
	}
}

namespace ImGui {
	[[maybe_unused]]
	static void writeText(
		const std::string& textString,
		ImVec2 textAlign   = ImVec2{ 0.5f, 0.5f },
		ImVec4 textColor   = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f },
		ImVec2 textPadding = ImVec2{ 6.0f, 6.0f }
	) {
		using namespace ImGui;
		const auto textPos{ (
			GetWindowSize() - CalcTextSize(textString.c_str()) - textPadding * 2
		) * textAlign + textPadding };

		PushStyleColor(ImGuiCol_Text, textColor);
		SetCursorPos(textPos);
		TextUnformatted(textString.c_str());
		PopStyleColor();
	}

	[[maybe_unused]]
	static void writeShadowedText(
		const std::string& textString,
		ImVec2 textAlign   = ImVec2{ 0.5f, 0.5f },
		ImVec4 textColor   = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f },
		ImVec2 textPadding = ImVec2{ 6.0f, 6.0f },
		ImVec2 shadowDist  = ImVec2{ 2.0f, 2.0f }
	) {
		using namespace ImGui;
		const auto textPos{ (
			GetWindowSize() - CalcTextSize(textString.c_str()) - textPadding * 2
		) * textAlign + textPadding };

		const auto shadowOffset{ shadowDist * 0.5f };
		PushStyleColor(ImGuiCol_Text, { 0.0f, 0.0f, 0.0f, 1.0f });
		SetCursorPos(textPos + shadowOffset);
		TextUnformatted(textString.c_str());
		PopStyleColor();

		PushStyleColor(ImGuiCol_Text, textColor);
		SetCursorPos(textPos - shadowOffset);
		TextUnformatted(textString.c_str());
		PopStyleColor();
	}

	static void DrawRotatedImage(void* texture, const ImVec2& dimensions, int rotation) {
		static constexpr ImVec2 TL{ 0, 0 };
		static constexpr ImVec2 TR{ 1, 0 };
		static constexpr ImVec2 BL{ 0, 1 };
		static constexpr ImVec2 BR{ 1, 1 };

		const auto pos = ImGui::GetCursorScreenPos();

		const ImVec2 A{ pos.x,                pos.y };
		const ImVec2 B{ pos.x + dimensions.x, pos.y };
		const ImVec2 C{ pos.x + dimensions.x, pos.y + dimensions.y };
		const ImVec2 D{ pos.x,                pos.y + dimensions.y };

		switch (rotation & 3) {
			case 0:
				ImGui::GetWindowDrawList()->AddImageQuad(
					reinterpret_cast<ImTextureID>(texture), A, B, C, D, TL, TR, BR, BL);
				break;

			case 1:
				ImGui::GetWindowDrawList()->AddImageQuad(
					reinterpret_cast<ImTextureID>(texture), A, B, C, D, BL, TL, TR, BR);
				break;

			case 2:
				ImGui::GetWindowDrawList()->AddImageQuad(
					reinterpret_cast<ImTextureID>(texture), A, B, C, D, BR, BL, TL, TR);
				break;

			case 3:
				ImGui::GetWindowDrawList()->AddImageQuad(
					reinterpret_cast<ImTextureID>(texture), A, B, C, D, TR, BR, BL, TL);
				break;
		}
		ImGui::Dummy(dimensions);
	}
}

/*==================================================================*/

bool FrontendInterface::mergeOverflowingWindows() noexcept {
	std::unique_lock lock{ sHooks->windows.overflow_lock };

	auto& src_windows = sHooks->windows.overflow.buffer;
	if (src_windows.empty()) { return false; }

	auto& dst_windows = sHooks->windows.registry.buffer;

	blog.newEntry<BLOG::DBG>("{} overflow windows found.", src_windows.size());

	dst_windows.insert(dst_windows.end(),
		std::make_move_iterator(src_windows.begin()),
		std::make_move_iterator(src_windows.end())
	);
	src_windows.clear();

	return true;
}

void FrontendInterface::invokeRegisteredWindows() noexcept {
	std::unique_lock lock{ sHooks->windows.registry_lock };

	auto& windows = sHooks->windows.registry;

	do {
		while (windows.offset < windows.buffer.size()) {
			auto& entry = windows.buffer[windows.offset];
			if (auto shared_ptr = entry.lock()) {
				(*shared_ptr)(); ++windows.offset;
			} else {
				windows.buffer.erase(windows.buffer.begin() + windows.offset);
			}
		}
	} while (mergeOverflowingWindows());

	windows.offset = 0;
}

bool FrontendInterface::mergeOverflowingMenus(const Key& tag) noexcept {
	std::unique_lock lock{ sHooks->menus.overflow_lock };

	auto& src_window = sHooks->menus.overflow[tag];
	if (src_window.empty()) {
		 return false; }

	unsigned migration_count = 0;
	for (auto& [menu_title, src_hooks] : src_window) {
		if (src_hooks.buffer.empty()) { continue; }
		auto& dst_hooks = sHooks->menus.registry[tag][menu_title];

		blog.newEntry<BLOG::DBG>("{} overflow hooks for menu \"{}\" found.",
			src_hooks.buffer.size(), menu_title);

		dst_hooks.buffer.insert(dst_hooks.buffer.end(),
			std::make_move_iterator(src_hooks.buffer.begin()),
			std::make_move_iterator(src_hooks.buffer.end())
		);
		src_hooks.buffer.clear();
		++migration_count;
	}

	return !!migration_count;
}

void FrontendInterface::invokeRegisteredMenus(const Key& tag) noexcept {
	std::unique_lock lock{ sHooks->menus.registry_lock };

	auto window = sHooks->menus.registry.find(tag);
	if (window == sHooks->menus.registry.end()) { return; }

	do {
		for (auto& [menu_title, hooks] : window->second) {
			if (hooks.buffer.empty()) { continue; }
			if (!ImGui::BeginMenu(menu_title.c_str())) { continue; }

			while (hooks.offset < hooks.buffer.size()) {
				auto& entry = hooks.buffer[hooks.offset];
				if (auto shared_ptr = entry.lock()) {
					(*shared_ptr)(); ++hooks.offset;
				} else {
					hooks.buffer.erase(hooks.buffer.begin() + hooks.offset);
				}
			}

			ImGui::EndMenu();
		}
	} while (mergeOverflowingMenus(tag));

	for (auto& [_, hooks] : window->second)
		{ hooks.offset = 0; } // reset for next frame
}

/*==================================================================*/

void FrontendInterface::SetScaleFactor(float scale) noexcept {
	ImGui::GetStyle().FontScaleMain = scale;
}
float& FrontendInterface::GetScaleFactor() noexcept {
	return ImGui::GetStyle().FontScaleMain;
}

/*==================================================================*/

static ImFont* sMainFont{};

void FrontendInterface::InitContext(const char* home_dir) {
	static Str ini_path = home_dir ? Str(home_dir) + "imgui.ini" : Str();
	static Str log_path = home_dir ? Str(home_dir) + "imgui.log" : Str();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();

	io.IniFilename = home_dir ? ini_path.c_str() : nullptr;
	io.LogFilename = home_dir ? log_path.c_str() : nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	sHooks = std::make_unique<RegistryAggregate>();

	sMainFont = io.Fonts->AddFontFromMemoryCompressedTTF
		(FontData::Roboto_Mono, std::size(FontData::Roboto_Mono), 16.0f);

	io.Fonts->Build();
	io.FontDefault = sMainFont;
}

void FrontendInterface::QuitContext() {
	ImGui::DestroyContext();
}

void FrontendInterface::InitVideo(SDL_Window* window, SDL_Renderer* renderer) {
	//UpdateFontScale();

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);
}

void FrontendInterface::QuitVideo() {
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
}

/*==================================================================*/

void FrontendInterface::ProcessEvent(void* event) {
	ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<SDL_Event*>(event));
}

void FrontendInterface::NewFrame() {
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();

	ImGui::NewFrame();

	sTaskbarSize = ImGui::GetFrameHeight();

	initializeMainDockspace();
	invokeRegisteredWindows();
}

void FrontendInterface::RenderFrame(SDL_Renderer* renderer) {
	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

/*==================================================================*/

void FrontendInterface::PrepareViewport(
	bool enable, bool integer_scaling,
	int width, int height, int rotation,
	const char* overlay_data, SDL_Texture* texture
) {
	ImGui::SetNextWindowDockID(sMainDockID, ImGuiCond_FirstUseEver);

	ImGui::Begin("ViewportFrame", nullptr,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	if (enable) {
		using namespace ImGui;
		const ImVec2 windowArea{ GetContentRegionAvail() };

		auto rawAspectRatio{ std::min(windowArea.x / width, windowArea.y / height) };
		const auto normalizedRatio{ std::max((integer_scaling
			? std::floor(rawAspectRatio)
			: rawAspectRatio
		), 1.0f) };

		const ImVec2 textureArea{ width * normalizedRatio, height * normalizedRatio };

		SetCursorPos(floor(GetCursorPos() + (windowArea - textureArea) / 2.0f));
		DrawRotatedImage(texture, textureArea, rotation);

		if (overlay_data) { writeShadowedText(overlay_data, { 0.0f, 1.0f }); }
	}

	ImGui::End();
}

void FrontendInterface::initializeMainDockspace() {
	const auto& viewport = *ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport.Pos);
	ImGui::SetNextWindowSize({
		viewport.Size.x,
		viewport.Size.y - sTaskbarSize
	});
	ImGui::SetNextWindowViewport(viewport.ID);

	ImGui::Begin("MainDockspace", nullptr,
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoSavedSettings);

	if (ImGui::BeginMainMenuBar()) {
		invokeRegisteredMenus("");

		ImGui::EndMainMenuBar();
	}

	sMainDockID = ImGui::GetID("MainDockspace");
	ImGui::DockSpace(sMainDockID, { 0.0f, 0.0f });
	ImGui::End();

	/*==================================================================*/

	ImGui::SetNextWindowPos({
		viewport.Pos.x,
		viewport.Pos.y + viewport.Size.y - ImGui::GetFrameHeight() * 2
	});
	ImGui::SetNextWindowSize({ viewport.Size.x, ImGui::GetFrameHeight() * 2 });
	ImGui::SetNextWindowViewport(viewport.ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
	ImGui::Begin("TaskbarDockspace", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
	);
	ImGui::PopStyleVar();

	sTaskbarDockID = ImGui::GetID("TaskbarDockspace");
	ImGui::DockSpace(sTaskbarDockID, { 0.0f, 0.0f },
		ImGuiDockNodeFlags_NoDockingInCentralNode | ImGuiDockNodeFlags_NoResize |
		ImGuiDockNodeFlags_NoDockingSplit
		//| ImGuiDockNodeFlags_NoUndocking
	);
	ImGui::End();
}
