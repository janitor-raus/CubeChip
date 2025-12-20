/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "DisplayWindow.hpp"
#include "FrontendInterface.hpp"
#include "BasicVideoSpec.hpp"
#include "BasicLogger.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

/*==================================================================*/

DisplayWindow::DisplayWindow(std::size_t W, std::size_t H, const char* name) noexcept
	: mWindowName(std::make_shared<std::string>(
		name ? name : "Unnamed Display"))
	, mCallable(std::make_shared<Callable>())
	, mRenderHook(FrontendInterface::registerWindow(
		[&]() noexcept { RenderDisplayWindow(); }))
	, mTexture(BasicVideoSpec::makeDisplayTexture(
		FrontendInterface::GetCurrentRenderer(), int(W), int(H)))
	, swapchain(sizeof(int), int(W), int(H))
	, metadata_staging(int(W), int(H))
{}

DisplayWindow DisplayWindow::Create(std::size_t W, std::size_t H, const char* name) noexcept {
	if (W <= std::size_t(0) || H <= std::size_t(0) || W > std::size_t(8192) || H > std::size_t(8192)) {
		blog.newEntry<BLOG::ERR>("Display W/H out of size bounds, clamping!");
		W = std::clamp(W, std::size_t(1), std::size_t(8192));
		H = std::clamp(H, std::size_t(1), std::size_t(8192));
	}

	return DisplayWindow(W, H, name ? name : "Unnamed Display");
}

void DisplayWindow::SetScreenRotation(int rotation) noexcept {
	mScreenRotation.store(rotation & 3, mo::relaxed);
}

void DisplayWindow::SetIntegerScaling(bool enable) noexcept {
	mIntegerScaling.store(enable, mo::relaxed);
}

void DisplayWindow::SetCallShaderPass(bool enable) noexcept {
	mCallShaderPass.store(enable, mo::relaxed);
}

void DisplayWindow::SetWindowName(std::string_view name) noexcept {
	mWindowName.store(std::make_shared<std::string>(name), mo::relaxed);
}

void DisplayWindow::SetOverlayCallable(Callable&& callable) noexcept {
	mCallable = std::make_shared<Callable>(std::move(callable));
}

void DisplayWindow::RenderDisplayWindow() noexcept {
	if (!mTexture) { return; }

	swapchain.present([&](auto frame) noexcept {
		const auto& metadata = frame.buffer.metadata;

		BasicVideoSpec::renderStreamTexture(
			FrontendInterface::GetCurrentRenderer(),
			mTexture, frame.buffer.data());

		FrontendInterface::SetNextWindowDockingTo(0, true);

		auto window_name = mWindowName.load(mo::relaxed);
		if (ImGui::Begin(window_name->c_str(), nullptr,
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoCollapse
		)) {
			const auto rotation = mScreenRotation.load(mo::relaxed);
			const auto integer  = mIntegerScaling.load(mo::relaxed);
			const auto scaling  = float(metadata.get_scaling());
			const auto padding  = float(metadata.get_padding());

		// calc padding/borders spacing in advance
			const auto origin_point = ImGui::GetCursorPos();
			const auto display_zone = ImGui::GetContentRegionAvail();

			const auto borders_vec2 = ImVec2(metadata.border_width, metadata.border_width);
			const auto padding_vec2 = ImVec2(padding * 2, padding * 2);
			const auto content_zone = display_zone - borders_vec2;

		// calc texture dimensions
			const auto   base_texture_dimensions =
				metadata.get_flipped_viewport_if(rotation & 1);

			const auto norm_texture_dimensions = ImVec2(
				float(base_texture_dimensions.w),
				float(base_texture_dimensions.h)
			) * scaling;

		// calc AR values
			// raw aspect ratio of texture, with the floor clamped
			// to the texture's desired scale multiplier
			const auto base_AR = std::min(
				(content_zone.x - padding_vec2.x) / norm_texture_dimensions.x,
				(content_zone.y - padding_vec2.y) / norm_texture_dimensions.y
			);

			// normalized to integer scaling, if enabled
			const auto norm_AR = std::max(integer ? std::floor(base_AR) : base_AR, 1.0f);

		// calc drawing areas fixed by AR for consistency
			const auto texture_area = norm_texture_dimensions * norm_AR;
			const auto borders_area = texture_area + padding_vec2;

			if (padding >= 1) {
				ImGui::SetCursorPos(origin_point +
					ImGui::floor((display_zone - borders_area) * 0.5f));

				ImGui::DrawRectFilled(borders_area,
					padding, metadata.texture_tint.XBGR());
			}

			if (metadata.enabled) {
				const auto uv0 = ImVec2(
					base_texture_dimensions.x / float(metadata.get_base_frame().w),
					base_texture_dimensions.y / float(metadata.get_base_frame().h)
				);
				const auto uv1 = ImVec2(
					(base_texture_dimensions.x + base_texture_dimensions.w) / float(metadata.get_base_frame().w),
					(base_texture_dimensions.y + base_texture_dimensions.h) / float(metadata.get_base_frame().h)
				);

				ImGui::SetCursorPos(origin_point +
					ImGui::floor((display_zone - texture_area) * 0.5f));

				ImGui::DrawRotatedImage(mTexture, texture_area, rotation,
					uv0, uv1, RGBA(0xFF, 0xFF, 0xFF, metadata.texture_tint.A));
			}

			if (metadata.border_width >= 1.0f) {
				ImGui::SetCursorPos(origin_point +
					ImGui::floor((display_zone - borders_area) * 0.5f));

				ImGui::DrawRect(borders_area,
					metadata.border_width, padding,
					metadata.border_color.XBGR());
			}

			// apply dummy just in case
			ImGui::SetCursorPos(origin_point);
			ImGui::Dummy(display_zone);
			ImGui::SetCursorPos(origin_point);

			auto display_callable = mCallable.load(mo::relaxed);
			if (display_callable) { (*display_callable)(); }
		}
		ImGui::End();
	});
}

void SimpleStatOverlay(const std::string& overlay_data) noexcept {
	ImGui::writeShadowedText(overlay_data, RGBA::White, { 0.0f, 1.0f });
}
