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

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

/*==================================================================*/

namespace ImGui {
	[[maybe_unused]]
	inline auto clamp(const ImVec2& value, const ImVec2& min, const ImVec2& max) {
		return ImVec2{
			std::clamp(value.x, min.x, max.x),
			std::clamp(value.y, min.y, max.y)
		};
	}

	[[maybe_unused]]
	inline auto abs(const ImVec2& value) {
		return ImVec2{ std::abs(value.x), std::abs(value.y) };
	}

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

		const auto pos{ ImGui::GetCursorScreenPos() };

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

void FrontendInterface::Initialize(SDL_Window* window, SDL_Renderer* renderer) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().LogFilename = nullptr;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	//UpdateFontScale();

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);
}

void FrontendInterface::Shutdown() {
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void FrontendInterface::ProcessEvent(void* event) {
	ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<SDL_Event*>(event));
}

void FrontendInterface::NewFrame() {
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void FrontendInterface::RenderFrame(SDL_Renderer* renderer) {
	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

float FrontendInterface::GetFrameHeight() {
	return ImGui::GetFrameHeight();
}

void FrontendInterface::UpdateFontScale(const void* data, int size, float scale) {
	static auto currentScale{ 0.0f };

	if (scale < 1.0f) { return; }
	if (std::fabs(currentScale - scale) > 0.0f) {
		currentScale = scale;
		auto& io{ ImGui::GetIO() };

		if (data && size) {
			io.Fonts->AddFontFromMemoryCompressedTTF(data, size, scale * 17.0f);
		} else {
			ImFontConfig fontConfig;
			fontConfig.SizePixels = 16.0f * scale;

			io.Fonts->Clear();
			io.Fonts->AddFontDefault(&fontConfig);
		}
		ImGui::GetStyle().ScaleAllSizes(scale);
	}
}

void FrontendInterface::PrepareViewport(
	bool enable, bool integer_scaling,
	int width, int height, int rotation,
	const char* overlay_data, SDL_Texture* texture
) {
	const auto viewportFrameDimensions{ ImVec2{
		ImGui::GetIO().DisplaySize.x,
		ImGui::GetIO().DisplaySize.y - ImGui::GetFrameHeight()
	} };

	ImGui::SetNextWindowSize(viewportFrameDimensions);
	ImGui::SetNextWindowPos({ 0.0f, ImGui::GetFrameHeight() });

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
	ImGui::Begin("ViewportFrame", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBringToFrontOnFocus
	);
	ImGui::PopStyleVar();

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File"))
		{
			if (auto OpenFile{ FnHook_OpenFile.load(mo::relaxed) })
				{ if (ImGui::MenuItem("Open File...")) { OpenFile(); } }

			if (auto OpenDataDir{ FnHook_OpenDataDir.load(mo::relaxed) })
				{ if (ImGui::MenuItem("Open Data Folder...")) { OpenDataDir(); } }

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (enable) {
		const auto aspectRatio{ integer_scaling
			? std::floor(std::min(
				viewportFrameDimensions.x / width,
				viewportFrameDimensions.y / height
			))
			: std::min(
				viewportFrameDimensions.x / width,
				viewportFrameDimensions.y / height
			)
		};

		const auto viewportDimensions{ ImVec2{
			width  * std::max(aspectRatio, 1.0f),
			height * std::max(aspectRatio, 1.0f)
		} };

		const auto viewportOffsets{ (viewportFrameDimensions - viewportDimensions) / 2.0f };

		if (viewportOffsets.x > 0.0f) { ImGui::SetCursorPosX(std::floor(ImGui::GetCursorPosX() + viewportOffsets.x)); }
		if (viewportOffsets.y > 0.0f) { ImGui::SetCursorPosY(std::floor(ImGui::GetCursorPosY() + viewportOffsets.y)); }

		ImGui::DrawRotatedImage(texture, viewportDimensions, rotation);

		if (overlay_data) { ImGui::writeShadowedText(overlay_data, { 0.0f, 1.0f }); }
	}

	ImGui::End();
}

void FrontendInterface::PrepareGeneralUI() {
	//static bool show_demo_window{ true };
	//if (show_demo_window) {
	//	ImGui::ShowDemoWindow(&show_demo_window);
	//}
}
