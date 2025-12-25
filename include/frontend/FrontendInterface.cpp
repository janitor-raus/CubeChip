/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <cmath>
#include <algorithm>
#include <string>
#include <numeric>

#include "FrontendInterface.hpp"
#include "HomeDirManager.hpp"
#include "BasicLogger.hpp"

#include "fonts/RobotoMono.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

/*==================================================================*/

Vec2::Vec2(const ImVec2& vec2) noexcept
	: x(vec2.x), y(vec2.y)
{}

Vec2::Vec2(ImVec2&& vec2) noexcept
	: x(vec2.x), y(vec2.y)
{}

Vec2::operator ImVec2() const noexcept {
	return ImVec2(x, y);
}

Vec4::Vec4(const ImVec4& vec4) noexcept
	: x(vec4.x), y(vec4.y), z(vec4.z), w(vec4.w)
{}

Vec4::operator ImVec4() const noexcept {
	return ImVec4(x, y, z, w);
}

/*==================================================================*/

static ImGuiID sMainDockID{};

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
	void TextUnformatted(const char* text, unsigned color, const char* text_end) noexcept {
		PushStyleColor(ImGuiCol_Text, color);
		TextUnformatted(text, text_end);
		PopStyleColor();
	}

	void writeText(
		const std::string& textString, RGBA textColor,
		Vec2 textAlign, Vec2 textPadding
	) noexcept {
		using namespace ImGui;
		const auto textPos = GetCursorPos() + textPadding + textAlign * (
			GetContentRegionAvail() - CalcTextSize(textString.c_str()) - textPadding * 2);

		SetCursorPos(textPos);
		TextUnformatted(textString.c_str(), textColor);
	}

	void writeShadowedText(
		const std::string& textString, RGBA textColor,
		Vec2 textAlign, Vec2 textPadding, Vec2 shadowDist
	) noexcept {
		using namespace ImGui;
		const auto textPos = GetCursorPos() + textPadding + textAlign * (
			GetContentRegionAvail() - CalcTextSize(textString.c_str()) - textPadding * 2);
		const auto shadowOffset = shadowDist * 0.5f;

		SetCursorPos(textPos + shadowOffset);
		TextUnformatted(textString.c_str(), IM_COL32_BLACK);

		SetCursorPos(textPos - shadowOffset);
		TextUnformatted(textString.c_str(), textColor);
	}

	void DrawRotatedImage(
		void* texture, const ImVec2& dims, unsigned rotation,
		const ImVec2& uv0, const ImVec2& uv1, unsigned tint
	) noexcept {
		if (!texture) { return; }

		const ImVec2 TL = ImGui::GetCursorScreenPos();
		const ImVec2 TR = { TL.x + dims.x, TL.y          };
		const ImVec2 BR = { TL.x + dims.x, TL.y + dims.y };
		const ImVec2 BL = { TL.x,          TL.y + dims.y };

		static constexpr int rotLUT[4][4] = {
			{ 0, 1, 3, 2 }, //   0 deg : TL TR BR BL
			{ 3, 0, 1, 2 }, //  90 deg
			{ 2, 3, 0, 1 }, // 180 deg
			{ 1, 2, 3, 0 }, // 270 deg
		};

		const ImVec2 uvs[] = {
			{ uv0.x, uv0.y }, // TL
			{ uv1.x, uv0.y }, // TR
			{ uv0.x, uv1.y }, // BL
			{ uv1.x, uv1.y }, // BR
		};
		const auto& m = rotLUT[rotation & 3];

		ImGui::GetWindowDrawList()->AddImageQuad(
			reinterpret_cast<ImTextureID>(texture), TL, TR, BR, BL,
			uvs[m[0]], uvs[m[1]], uvs[m[2]], uvs[m[3]], tint
		);
	}

	void DrawRect(const ImVec2& dims, float width, float round, unsigned color) noexcept {
		const auto origin = ImGui::GetCursorScreenPos();

		ImGui::GetWindowDrawList()->AddRect(origin, origin + dims,
			color, round, ImDrawFlags_RoundCornersAll, width);
	}

	void DrawRectFilled(const ImVec2& dims, float round, unsigned color) noexcept {
		const auto origin = ImGui::GetCursorScreenPos();

		ImGui::GetWindowDrawList()->AddRectFilled(origin, origin + dims,
			color, round, ImDrawFlags_RoundCornersAll);
	}
}

/*==================================================================*/

bool FrontendInterface::merge_overflowing_windows() noexcept {
	std::unique_lock lock(s_hooks->windows.overflow_lock);

	auto& src_windows = s_hooks->windows.overflow.buffer;
	if (src_windows.empty()) { return false; }

	auto& dst_windows = s_hooks->windows.registry.buffer;

	blog.newEntry<BLOG::DBG>("{} overflow windows found.", src_windows.size());

	dst_windows.insert(dst_windows.end(),
		std::make_move_iterator(src_windows.begin()),
		std::make_move_iterator(src_windows.end())
	);
	src_windows.clear();

	return true;
}

void FrontendInterface::invoke_registered_windows() noexcept {
	std::unique_lock lock(s_hooks->windows.registry_lock);

	auto& windows = s_hooks->windows.registry;

	do {
		while (windows.offset < windows.buffer.size()) {
			auto& entry = windows.buffer[windows.offset];
			if (auto shared_ptr = entry.lock()) {
				(*shared_ptr)(); ++windows.offset;
			} else {
				windows.buffer.erase(windows.buffer.begin() + windows.offset);
			}
		}
	} while (merge_overflowing_windows());

	windows.offset = 0;
}

bool FrontendInterface::merge_overflowing_menus(const PlainKey& tag) noexcept {
	std::unique_lock lock(s_hooks->menus.overflow_lock);

	auto& src_window = s_hooks->menus.overflow[tag];
	if (src_window.empty()) {
		 return false; }

	unsigned migration_count = 0;
	for (auto& [menu_title, src_hooks] : src_window) {
		if (src_hooks.buffer.empty()) { continue; }
		auto& dst_hooks = s_hooks->menus.registry[tag][menu_title];

		blog.newEntry<BLOG::DBG>("{} overflow hooks for menu \"{}\" found.",
			src_hooks.buffer.size(), menu_title.second.c_str());

		dst_hooks.buffer.insert(dst_hooks.buffer.end(),
			std::make_move_iterator(src_hooks.buffer.begin()),
			std::make_move_iterator(src_hooks.buffer.end())
		);
		src_hooks.buffer.clear();
		++migration_count;
	}

	return !!migration_count;
}

void FrontendInterface::invoke_registered_menus(const PlainKey& tag) noexcept {
	std::unique_lock lock(s_hooks->menus.registry_lock);

	auto window = s_hooks->menus.registry.find(tag);
	if (window == s_hooks->menus.registry.end()) { return; }

	do {
		for (auto& [menu_title, hooks] : window->second) {
			if (hooks.buffer.empty()) { continue; }

			if (ImGui::BeginMenu(menu_title.second.c_str())) {
				hooks.first_hit = !std::exchange(hooks.has_focus, true);
				s_active_menu = &hooks;

				while (hooks.offset < hooks.buffer.size()) {
					auto& entry = hooks.buffer[hooks.offset];
					if (auto shared_ptr = entry.lock()) {
						(*shared_ptr)(); ++hooks.offset;
					} else {
						hooks.buffer.erase(hooks.buffer.begin() + hooks.offset);
					}
				}

				ImGui::EndMenu();
			} else {
				s_active_menu = nullptr;
				hooks.has_focus = false;
				continue;
			}
		}
	} while (merge_overflowing_menus(tag));

	for (auto& [_, hooks] : window->second)
		{ hooks.offset = 0; } // reset for next frame
}

/*==================================================================*/

void FrontendInterface::set_ui_scale_factor(float scale) noexcept {
	ImGui::GetStyle().FontScaleMain = scale;
}
float FrontendInterface::get_ui_scale_factor() noexcept {
	return ImGui::GetStyle().FontScaleMain;
}

/*==================================================================*/

static ImFont* sMainFont{};

void FrontendInterface::init_context(const char* home_dir) {
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

	s_hooks = std::make_unique<RegistryAggregate>();

	sMainFont = io.Fonts->AddFontFromMemoryCompressedTTF
		(FontData::Roboto_Mono, std::size(FontData::Roboto_Mono), 16.0f);

	io.FontDefault = sMainFont;
}

void FrontendInterface::quit_context() {
	ImGui::DestroyContext();
}

void FrontendInterface::init_video(SDL_Window* window, SDL_Renderer* renderer) {
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);
	s_current_renderer = renderer;
}

void FrontendInterface::quit_video() {
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
}

/*==================================================================*/

void FrontendInterface::process_event(void* event) {
	ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<SDL_Event*>(event));
}

void FrontendInterface::begin_new_frame() {
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();

	ImGui::NewFrame();

	if (ImGui::BeginMainMenuBar()) {
		invoke_registered_menus("");
		ImGui::EndMainMenuBar();
	}

	const auto& viewport = *ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport.Pos.x, viewport.Pos.y + ImGui::GetFrameHeight()));
	ImGui::SetNextWindowSize(ImVec2(viewport.Size.x, viewport.Size.y - ImGui::GetFrameHeight()));
	ImGui::SetNextWindowViewport(viewport.ID);

	ImGui::Begin("MainDockspace", nullptr, ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

	sMainDockID = ImGui::GetID("MainDockspace");
	ImGui::DockSpace(sMainDockID);
	ImGui::End();

	invoke_registered_windows();
}

void FrontendInterface::render_frame(SDL_Renderer* renderer) {
	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void FrontendInterface::dock_next_window_to(unsigned id, bool first_time) noexcept {
	ImGui::SetNextWindowDockID(id ? id : sMainDockID,
		first_time ? ImGuiCond_FirstUseEver : ImGuiCond_Always);
}
