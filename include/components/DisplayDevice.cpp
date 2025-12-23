/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "DisplayDevice.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "FrontendInterface.hpp"
#include "AtomSharedPtr.hpp"
#include "BasicVideoSpec.hpp"
#include "BasicLogger.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

/*==================================================================*/

struct DisplayDevice::DisplayContext {
	using Callable = DisplayDevice::Callable;

	AtomSharedPtr<std::string> m_display_name;
	AtomSharedPtr<Callable>    m_osd_callable;
	FrontendInterface::Hook    m_render_hook;
	SDL_Holder<SDL_Texture>    m_sdl_texture;
	DisplayDevice::Swapchain   m_swapchain;

	std::atomic<int>  m_screen_rotation{};
	std::atomic<bool> m_integer_scaling{};
	std::atomic<bool> m_utilize_shaders{};

	DisplayContext(std::size_t W, std::size_t H, const char* name, std::size_t bpp) noexcept
		: m_display_name(std::make_shared<std::string>(name))
		, m_osd_callable(std::make_shared<Callable>())
		, m_render_hook(FrontendInterface::register_window(
			[&]() noexcept { render_display_window(); }))
		, m_sdl_texture(BasicVideoSpec::makeDisplayTexture(
			FrontendInterface::get_current_renderer(),
			static_cast<int>(W), static_cast<int>(H)))
		, m_swapchain(bpp, static_cast<int>(W), static_cast<int>(H))
	{}

private:
	void render_display_window() noexcept {
		m_swapchain.present([&](auto frame) noexcept {
			const auto& metadata = frame.buffer.metadata;

			BasicVideoSpec::renderStreamTexture(
				FrontendInterface::get_current_renderer(),
				m_sdl_texture, frame.buffer.data());

			FrontendInterface::dock_next_window_to(0, true);

			auto window_name = m_display_name.load(mo::relaxed);
			if (ImGui::Begin(window_name->c_str(), nullptr,
				ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoCollapse
			)) {
				const auto rotation = m_screen_rotation.load(mo::relaxed);
				const auto integer  = m_integer_scaling.load(mo::relaxed);
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
					ImGui::SetCursorPos(origin_point + ImGui::floor(
						(display_zone - borders_area) * 0.5f));

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

					ImGui::SetCursorPos(origin_point + ImGui::floor(
						(display_zone - texture_area) * 0.5f));

					ImGui::DrawRotatedImage(m_sdl_texture, texture_area, rotation,
						uv0, uv1, RGBA(0xFF, 0xFF, 0xFF, metadata.texture_tint.A));
				}

				if (metadata.border_width >= 1.0f) {
					ImGui::SetCursorPos(origin_point + ImGui::floor(
						(display_zone - borders_area) * 0.5f));

					ImGui::DrawRect(borders_area,
						metadata.border_width, padding,
						metadata.border_color.XBGR());
				}

				// apply dummy just in case
				ImGui::SetCursorPos(origin_point);
				ImGui::Dummy(display_zone);
				ImGui::SetCursorPos(origin_point);

				auto osd_callable = m_osd_callable.load(mo::relaxed);
				if (osd_callable) { (*osd_callable)(); }
			}
			ImGui::End();
		});
	}
};

/*==================================================================*/

DisplayDevice::DisplayDevice(std::size_t W, std::size_t H, const char* name, std::size_t bpp, int) noexcept
	: m_context(std::make_unique<DisplayContext>(W, H, name, bpp))
	, metadata_staging(static_cast<int>(W), static_cast<int>(H))
{}

DisplayDevice::DisplayDevice(std::size_t W, std::size_t H, const char* name, std::size_t bpp) noexcept
	: DisplayDevice(
		std::clamp(W, std::size_t(1), std::size_t(8192)),
		std::clamp(H, std::size_t(1), std::size_t(8192)),
		name ? name : "Unnamed Display",
		std::clamp(bpp, std::size_t(1), std::size_t(6)), 0
	)
{
	if (metadata_staging.get_base_frame().area() != (W * H)) {
		blog.newEntry<BLOG::ERR>("Display W/H out of size bounds, clamping!");
	}
}

DisplayDevice::~DisplayDevice() noexcept = default;
DisplayDevice::DisplayDevice(DisplayDevice&&) noexcept = default;
DisplayDevice& DisplayDevice::operator=(DisplayDevice&&) noexcept = default;

/*==================================================================*/

auto DisplayDevice::swapchain()       noexcept ->       Swapchain& { return m_context->m_swapchain; }
auto DisplayDevice::swapchain() const noexcept -> const Swapchain& { return m_context->m_swapchain; }

/*==================================================================*/

void DisplayDevice::set_screen_rotation(int rotation) noexcept {
	m_context->m_screen_rotation.store(rotation & 3, mo::relaxed);
}

void DisplayDevice::set_integer_scaling(bool enable) noexcept {
	m_context->m_integer_scaling.store(enable, mo::relaxed);
}

void DisplayDevice::set_utilize_shaders(bool enable) noexcept {
	m_context->m_utilize_shaders.store(enable, mo::relaxed);
}

void DisplayDevice::set_display_name(std::string_view name) noexcept {
	m_context->m_display_name.store(std::make_shared<std::string>(name), mo::relaxed);
}

void DisplayDevice::set_osd_callable(Callable&& callable) noexcept {
	m_context->m_osd_callable.store(std::make_shared<Callable>(std::move(callable)), mo::relaxed);
}

/*==================================================================*/

void osd::simple_stat_overlay(const std::string& overlay_data) noexcept {
	ImGui::writeShadowedText(overlay_data, RGBA::White, { 0.0f, 1.0f });
}
