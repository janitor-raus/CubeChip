/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "DisplayDevice.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "AtomicBox.hpp"
#include "BasicVideoSpec.hpp"

#include <imgui.h>

/*==================================================================*/

struct DisplayDevice::DisplayContext {
	using Callable = DisplayDevice::Callable;
	using Metadata = FramePacket::Metadata;

	SDL_Renderer* const      m_renderer;
	DisplayDevice::Swapchain m_swapchain;
	SDL_Unique<SDL_Texture>  m_stream_texture;
	SDL_Unique<SDL_Texture>  m_target_texture;

	AtomicBox<Metadata>      m_staging_data;
	AtomSharedPtr<Callable>  m_osd_callable;

	ez::Frame m_old_target_size{};

	BoundedParam<0, 0, 3> m_screen_rotation;

	bool m_integer_scaling{};
	bool m_utilize_shaders{};
	bool m_debugging_stats{};

public:
	DisplayContext(std::size_t W, std::size_t H, SDL_Renderer* renderer) noexcept
		: m_renderer(renderer), m_swapchain(int(W), int(H))
		, m_stream_texture(BasicVideoSpec::create_stream_texture(m_renderer, int(W), int(H)))
		, m_staging_data(std::make_shared<Metadata>(int(W), int(H)))
	{
		IM_ASSERT((m_staging_data.read()->get_base_frame().area() == (W * H))
			&& "Display W/H sizes are beyond expected bounds, clamping!");
	}

public:
	void render_display() noexcept {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);
		ImGui::PushID(this);
		const bool visible = ImGui::BeginChild("##display_child");
		ImGui::PopStyleColor();

		if (!visible) {
			ImGui::EndChild();
			ImGui::PopID();
			return;
		}

		m_swapchain.present([&](auto frame) noexcept {
			if (frame.dirty) {
				BasicVideoSpec::write_stream_texture(
					m_renderer, m_stream_texture, frame.buffer.data());
			}

			const auto metadata_view = m_staging_data.read();

			const auto& metadata = metadata_view->debug_flags
				? *metadata_view : frame.buffer.metadata;

			const auto px_ratio = float(metadata.pixel_ratio);
			const auto min_zoom = float(metadata.minimum_zoom);
			const auto margin   = float(metadata.inner_margin);
			const auto rounding = ImGui::GetStyle().WindowRounding * 2.0f;

			const auto cur_viewport = metadata.get_viewport();
			const auto dar_viewport = (*m_screen_rotation & 1)
				? ez::Rect(cur_viewport.y, cur_viewport.x,
					int(cur_viewport.h * px_ratio), cur_viewport.w)
				: ez::Rect(cur_viewport.x, cur_viewport.y,
					int(cur_viewport.w * px_ratio), cur_viewport.h);

			const auto border_width = metadata.border_width / 2 * 2;
			const auto borders_vec2 = ImVec2(float(border_width), float(border_width));
			const auto padding_vec2 = ImVec2(margin * 2, margin * 2) + borders_vec2;

		// calc padding/borders spacing in advance
			const auto origin_point = ImGui::GetCursorPos();
			const auto display_zone = ImGui::GetContentRegionAvail();
			const auto content_zone = display_zone - borders_vec2;

		// calc AR values
			// raw aspect ratio of texture, with the floor clamped
			// to the texture's desired scale multiplier
			const auto base_AR = std::min(
				(content_zone.x - padding_vec2.x) / dar_viewport.w,
				(content_zone.y - padding_vec2.y) / dar_viewport.h
			);

		// calc drawing areas fixed by AR for consistency
			const auto texture_area = ImVec2(float(dar_viewport.w), float(dar_viewport.h))
				* std::max(m_integer_scaling ? std::floor(base_AR) : base_AR, min_zoom);
			const auto borders_area = texture_area + padding_vec2;

		// calc target texture size for integer nn -> fractional linear scaling
			const auto target_AR = std::max(std::floor(base_AR), min_zoom);

			const auto new_target_size = ez::Frame(
				int(std::ceil(dar_viewport.w * target_AR)),
				int(std::ceil(dar_viewport.h * target_AR))
			);

			if (metadata.enabled) {
				if (new_target_size != m_old_target_size) {
					m_target_texture.reset(BasicVideoSpec::create_target_texture(
						m_renderer, new_target_size.w, new_target_size.h, true));
					m_old_target_size = new_target_size;
				}

				BasicVideoSpec::write_stream_texture(
					m_renderer, m_target_texture, m_stream_texture);

				ImGui::SetCursorPos(origin_point + ImGui::floor(
					(display_zone - borders_area) * 0.5f));

				ImGui::DrawRectFilled(borders_area,
					rounding, metadata.texture_tint.XBGR());

				const auto uv0 = ImVec2(
					cur_viewport.x / float(metadata.get_base_frame().w),
					cur_viewport.y / float(metadata.get_base_frame().h)
				);
				const auto uv1 = ImVec2(
					(cur_viewport.x + cur_viewport.w) / float(metadata.get_base_frame().w),
					(cur_viewport.y + cur_viewport.h) / float(metadata.get_base_frame().h)
				);

				ImGui::SetCursorPos(origin_point + ImGui::floor(
					(display_zone - texture_area) * 0.5f));

				ImGui::DrawRotatedImage(m_target_texture, texture_area, *m_screen_rotation,
					uv0, uv1, RGBA(0xFF, 0xFF, 0xFF, metadata.texture_tint.A).ABGR());
			}

			if (metadata.border_width != 0) {
				ImGui::SetCursorPos(origin_point + ImGui::floor(
					(display_zone - borders_area) * 0.5f));

				ImGui::DrawRect(borders_area, float(metadata.border_width),
					rounding, metadata.border_color.XBGR());
			}

			// apply dummy just in case
			ImGui::SetCursorPos(origin_point);
			ImGui::Dummy(display_zone);
			ImGui::SetCursorPos(origin_point);

			auto osd_callable = m_osd_callable.load(std::memory_order::relaxed);
			if (osd_callable) { (*osd_callable)(); }

			if (m_debugging_stats) {
				ImGui::SetCursorPos(origin_point);

				if (ImGui::BeginTable("##debug_table", 2,
					ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody
				)) {
					static auto row = [](const char* label, auto fmt, auto... args) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(label);
						ImGui::TableSetColumnIndex(1);
						ImGui::Text(fmt, args...);
					};
					static auto row_separate = []() {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight() * 0.3f));
					};

					const auto window_size = ImGui::GetWindowSize();
					const auto usable_size = display_zone;

					const auto deco_margin = ImGui::GetWindowDecoSize();
					const auto constraints =
						ImVec2(float(dar_viewport.w), float(dar_viewport.h)) *
							min_zoom + padding_vec2 + borders_vec2 + deco_margin;


					row("Window size:", "%.0f x %.0f", window_size.x, window_size.y);
					row("Deco size:",   "%.0f x %.0f", deco_margin.x, deco_margin.y);
					row("Constrained:", "%.0f x %.0f", constraints.x, constraints.y);
					row("Usable size",  "%.0f x %.0f", usable_size.x, usable_size.y);
					row_separate();
					row("Viewport (w,h):", "%d x %d", dar_viewport.w, dar_viewport.h);
					row("Viewport (x,y):", "%d , %d", dar_viewport.x, dar_viewport.y);
					row_separate();
					row("Base AR:",   "%.3f", base_AR);
					row("Target AR:", "%.3f", target_AR);
					row_separate();
					row("Texture area size:", "%.0f x %.0f", texture_area.x, texture_area.y);
					row("Borders area size:", "%.0f x %.0f", borders_area.x, borders_area.y);
					row_separate();
					row("Texture scaled as:", "%d x %d", new_target_size.w, new_target_size.h);

					ImGui::EndTable();
				}
			}
		});
		ImGui::EndChild();
		ImGui::PopID();
	}
	void render_settings_menu() noexcept {
		if (!ImGui::BeginMenu("Display Settings")) { return; }
		auto metadata = m_staging_data.edit();

		ImGui::SliderInt("Border Width", &*metadata->border_width,
			metadata->border_width.min,
			metadata->border_width.max,
			"%d", ImGuiSliderFlags_AlwaysClamp
		);

		ImGui::SliderInt("Inner Margin", &*metadata->inner_margin,
			metadata->inner_margin.min,
			metadata->inner_margin.max,
			"%d", ImGuiSliderFlags_AlwaysClamp
		);

		const auto border_color = metadata->border_color;
		float border_color_c[] = {
			border_color.R / 255.0f, border_color.G / 255.0f,
			border_color.B / 255.0f, 1.0f
		};
		if (ImGui::ColorEdit3("Border Color", border_color_c)) {
			metadata->border_color = RGBA(
				u8(border_color_c[0] * 255.0f), u8(border_color_c[1] * 255.0f),
				u8(border_color_c[2] * 255.0f), u8(255)
			);
		}

		const auto texture_tint = metadata->texture_tint;
		float texture_tint_c[] = {
			texture_tint.R / 255.0f, texture_tint.G / 255.0f,
			texture_tint.B / 255.0f, texture_tint.A / 255.0f
		};
		if (ImGui::ColorEdit4("Texture Tint", texture_tint_c, ImGuiColorEditFlags_AlphaPreviewHalf)) {
			metadata->texture_tint = RGBA(
				u8(texture_tint_c[0] * 255.0f), u8(texture_tint_c[1] * 255.0f),
				u8(texture_tint_c[2] * 255.0f), u8(texture_tint_c[3] * 255.0f)
			);
		}

		ImGui::SliderInt("Minimum Zoom", &*metadata->minimum_zoom,
			metadata->minimum_zoom.min,
			metadata->minimum_zoom.max,
			"%d", ImGuiSliderFlags_AlwaysClamp
		);

		ImGui::SliderFloat("Pixel Ratio", &*metadata->pixel_ratio,
			metadata->pixel_ratio.min,
			metadata->pixel_ratio.max,
			"%.2f", ImGuiSliderFlags_AlwaysClamp
		);

		int integer_scaling = m_integer_scaling;
		static const char* scaling_labels[] = { "Fractional", "Integer" };
		if (ImGui::Combo("Screen Scaling", &integer_scaling, scaling_labels, 2)) {
			m_integer_scaling = integer_scaling != 0;
		}

		static const char* rotation_labels[] = { "0 degrees", "90 degrees", "180 degrees", "270 degrees" };
		ImGui::Combo("Screen Rotation", &*m_screen_rotation, rotation_labels, 4);

		int utilize_shaders = m_utilize_shaders;
		if (ImGui::SliderInt("Shaders Enabled?", &utilize_shaders,
			0, 1, "", ImGuiSliderFlags_NoInput
		)) {
			m_utilize_shaders = utilize_shaders != 0;
		}

		int enable_screen = metadata->enabled ? 1 : 0;
		if (ImGui::SliderInt("Screen Enabled?", &enable_screen,
			0, 1, "", ImGuiSliderFlags_NoInput
		)) {
			metadata->enabled = enable_screen != 0;
		}

		int debugging_stats = m_debugging_stats ? 1 : 0;
		if (ImGui::SliderInt("Debugging Stats?", &debugging_stats,
			0, 1, "", ImGuiSliderFlags_NoInput
		)) {
			m_debugging_stats = debugging_stats != 0;
		}

		ImGui::EndMenu();
	}
};

/*==================================================================*/

DisplayDevice::DisplayDevice(std::size_t W, std::size_t H, void* sdl_renderer) noexcept
	: m_context(std::make_unique<DisplayContext>(
		std::clamp<std::size_t>(W, 1u, 8192u),
		std::clamp<std::size_t>(H, 1u, 8192u),
		reinterpret_cast<SDL_Renderer*>(sdl_renderer)
	))
{}

DisplayDevice::~DisplayDevice() noexcept = default;

DisplayDevice::DisplayDevice(DisplayDevice&&) noexcept = default;
DisplayDevice& DisplayDevice::operator=(DisplayDevice&&) noexcept = default;

/*==================================================================*/

auto DisplayDevice::swapchain()       noexcept ->       Swapchain& { return m_context->m_swapchain; }
auto DisplayDevice::swapchain() const noexcept -> const Swapchain& { return m_context->m_swapchain; }

auto DisplayDevice::read_metadata() noexcept -> ScopedRead {
	return m_context->m_staging_data.read();
}

auto DisplayDevice::edit_metadata() noexcept -> ScopedEdit {
	return m_context->m_staging_data.edit();
}

/*==================================================================*/

int  DisplayDevice::get_screen_rotation() const noexcept {
	return m_context->m_screen_rotation;
}

void DisplayDevice::set_screen_rotation(int rotation) noexcept {
	m_context->m_screen_rotation = rotation & 3;
}

bool DisplayDevice::get_integer_scaling() const noexcept {
	return m_context->m_integer_scaling;
}

void DisplayDevice::set_integer_scaling(bool enable) noexcept {
	m_context->m_integer_scaling = enable;
}

bool DisplayDevice::get_utilize_shaders() const noexcept {
	return m_context->m_utilize_shaders;
}

void DisplayDevice::set_utilize_shaders(bool enable) noexcept {
	m_context->m_utilize_shaders = enable;
}

void DisplayDevice::render_display() noexcept {
	m_context->render_display();
}

void DisplayDevice::render_settings_menu() noexcept {
	m_context->render_settings_menu();
}

void DisplayDevice::set_osd_callable(Callable callable) noexcept {
	m_context->m_osd_callable.store(std::make_shared<Callable>
		(std::move(callable)), std::memory_order::relaxed);
}
