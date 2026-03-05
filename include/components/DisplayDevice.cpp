/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "DisplayDevice.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "AtomSharedPtr.hpp"
#include "BasicVideoSpec.hpp"
#include "BasicLogger.hpp"

#include <imgui.h>

/*==================================================================*/

#define RENDERER FrontendInterface::get_current_renderer()

struct DisplayDevice::DisplayContext {
	using Callable = DisplayDevice::Callable;

	AtomSharedPtr<ImLabel>   m_window_label;
	AtomSharedPtr<Callable>  m_osd_callable;
	SDL_Unique<SDL_Texture>  m_stream_texture;
	SDL_Unique<SDL_Texture>  m_target_texture;
	DisplayDevice::Swapchain m_swapchain;
	FramePacket::Metadata    m_staging_data;

	FrontendInterface::Hook  m_settings_menu_hook;
	FrontendInterface::Hook  m_debugger_menu_hook;
	FrontendInterface::Hook  m_render_window_hook;

	ez::Frame m_old_target_size{};

	bool* m_window_state_out = nullptr; // wire pointer to detect window closure
	bool* m_window_focus_out = nullptr; // write pointer to detect window focus
	bool  m_disable_menubar = true;
	bool  m_view_debug_menu = false;
	bool  m_fullscreen_mode = false;

	int   m_screen_rotation{};
	bool  m_integer_scaling{};
	bool  m_utilize_shaders{};

	DisplayContext(std::size_t W, std::size_t H, ImLabel name, std::size_t bpp) noexcept
		: m_window_label(std::make_shared<ImLabel>(std::move(name)))
		, m_osd_callable(nullptr)
		, m_stream_texture(BasicVideoSpec::create_stream_texture(RENDERER, int(W), int(H)))
		, m_swapchain(bpp, int(W), int(H))
		, m_staging_data(int(W), int(H))
		, m_settings_menu_hook(bind_settings_menu())
		, m_debugger_menu_hook(nullptr)
		, m_render_window_hook(FrontendInterface::register_window([&]() noexcept { render_window(); }))
	{
		if (m_staging_data.get_base_frame().area() != (W * H)) {
			blog.warn("Display W/H out of size bounds, clamping!");
		}
	}

private:
	void render_window() noexcept {
		m_swapchain.present([&](auto frame) noexcept {
			const auto& metadata = m_view_debug_menu
				? m_staging_data : frame.buffer.metadata;

			BasicVideoSpec::write_stream_texture(RENDERER,
				m_stream_texture, frame.buffer.data());

			const auto window_label = m_window_label.load(mo::relaxed);
			const auto window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
				| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
				| (m_disable_menubar ? ImGuiWindowFlags_None : ImGuiWindowFlags_MenuBar);

			const auto s_window_contents = [&]() noexcept {
				if (m_window_focus_out) {
					*m_window_focus_out = ImGui::IsWindowFocused
					(ImGuiFocusedFlags_RootAndChildWindows);
				}

				FrontendInterface::call_autohide_menubar(
					*window_label, m_disable_menubar);

				const auto px_ratio = float(metadata.pixel_ratio);
				const auto min_zoom = float(metadata.minimum_zoom);
				const auto margin   = float(metadata.inner_margin);
				const auto rounding = ImGui::GetStyle().WindowRounding * 2.0f;

				const auto cur_viewport = metadata.get_viewport();
				const auto dar_viewport = (m_screen_rotation & 1)
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
					int(dar_viewport.w * target_AR), int(dar_viewport.h * target_AR));

				if (metadata.enabled) {
					if (new_target_size != m_old_target_size) {
						m_target_texture.reset(BasicVideoSpec::create_target_texture(
							RENDERER, new_target_size.w, new_target_size.h, true));
						m_old_target_size = new_target_size;
					}

					BasicVideoSpec::write_stream_texture(
						RENDERER, m_target_texture, m_stream_texture);

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

					ImGui::DrawRotatedImage(m_target_texture, texture_area, m_screen_rotation,
						uv0, uv1, RGBA(0xFF, 0xFF, 0xFF, metadata.texture_tint.A).ABGR());
				}

				if (metadata.border_width >= 1.0f) {
					ImGui::SetCursorPos(origin_point + ImGui::floor(
						(display_zone - borders_area) * 0.5f));

					ImGui::DrawRect(borders_area,
						metadata.border_width, rounding,
						metadata.border_color.XBGR());
				}

				// apply dummy just in case
				ImGui::SetCursorPos(origin_point);
				ImGui::Dummy(display_zone);
				ImGui::SetCursorPos(origin_point);

				auto osd_callable = m_osd_callable.load(mo::relaxed);
				if (osd_callable) { (*osd_callable)(); }

				// toggle fullscreen mode on double-click
				if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow
					| ImGuiHoveredFlags_NoPopupHierarchy
					| ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
				) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					m_fullscreen_mode = !m_fullscreen_mode;
				}

				#ifdef DISPLAYDEVICEDEBUG
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
				#endif
			};

			ImGui::DockNextWindowTo(FrontendInterface::get_main_dockspace_id(), true);
			ImGui::SetNextWindowMinClientSize(ImVec2(float(480), float(360)));
			//ImGui::SetNextWindowMinClientSize(ImVec2(float(dar_viewport.w), float(dar_viewport.h))
			//	* min_zoom + padding_vec2 + borders_vec2); // old constraint

			if (ImGui::Begin(*window_label, m_window_state_out, window_flags)) {
				if (!m_fullscreen_mode) { s_window_contents(); }
			} else {
				if (m_window_focus_out) { *m_window_focus_out = false; }
				m_fullscreen_mode = false;
			}
			ImGui::End();

			if (m_fullscreen_mode) {
				const auto* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->Pos);
				ImGui::SetNextWindowSize(viewport->Size);

				const auto temp_label = ::join(window_label->get_id(), "_fullscreen");
				if (ImGui::Begin(temp_label.c_str(), nullptr, window_flags
					| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
				)) { s_window_contents(); }
				ImGui::End();
			}
		});
	}

	auto bind_settings_menu() noexcept -> FrontendInterface::Hook {
		return FrontendInterface::register_menu(*m_window_label.load(mo::relaxed),
		{ 89, "Settings" }, [&]() noexcept {{
			ImGui::Separator(2.0f);
			if (ImGui::Checkbox("Enable Debug Menu?", &m_view_debug_menu)) {
				if (m_view_debug_menu) {
					m_debugger_menu_hook = bind_debugger_menu();
				} else {
					m_debugger_menu_hook.reset();
				}
			}
		}});
	}

	auto bind_debugger_menu() noexcept -> FrontendInterface::Hook {
		return FrontendInterface::register_menu(*m_window_label.load(mo::relaxed),
		{ 90, "Debug" }, [&]() noexcept {{
			int value = m_staging_data.border_width;
			if (ImGui::SliderInt("Border Width", &value, m_staging_data.border_width.min,
				m_staging_data.border_width.max, "%d", ImGuiSliderFlags_AlwaysClamp
			)) {
				m_staging_data.border_width = value;
			}
		} {
			int value = m_staging_data.inner_margin;
			if (ImGui::SliderInt("Inner Margin", &value, m_staging_data.inner_margin.min,
				m_staging_data.inner_margin.max, "%d", ImGuiSliderFlags_AlwaysClamp
			)) {
				m_staging_data.inner_margin = value;
			}
		} {
			const auto value = m_staging_data.border_color;
			float color[3] = { (value.R / 255.0f), (value.G / 255.0f), (value.B / 255.0f) };
			if (ImGui::ColorEdit3("Border Color", color)) {
				m_staging_data.border_color = RGBA(
					u8(color[0] * 255.0f), u8(color[1] * 255.0f),
					u8(color[2] * 255.0f), u8(255)
				);
			}
		} {
			const auto value = m_staging_data.texture_tint;
			float color[4] = { (value.R / 255.0f), (value.G / 255.0f), (value.B / 255.0f), (value.A / 255.0f) };
			if (ImGui::ColorEdit4("Texture Tint", color, ImGuiColorEditFlags_AlphaPreviewHalf)) {
				m_staging_data.texture_tint = RGBA(
					u8(color[0] * 255.0f), u8(color[1] * 255.0f),
					u8(color[2] * 255.0f), u8(color[3] * 255.0f)
				);
			}
		} {
			int value = m_staging_data.minimum_zoom;
			if (ImGui::SliderInt("Minimum Zoom", &value, m_staging_data.minimum_zoom.min,
				m_staging_data.minimum_zoom.max, "%d", ImGuiSliderFlags_AlwaysClamp
			)) {
				m_staging_data.minimum_zoom = value;
			}
		} {
			float value = m_staging_data.pixel_ratio;
			if (ImGui::SliderFloat("Pixel Ratio", &value, m_staging_data.pixel_ratio.min,
				m_staging_data.pixel_ratio.max, "%.2f", ImGuiSliderFlags_AlwaysClamp
			)) {
				m_staging_data.pixel_ratio = value;
			}
		} {
			int value = m_integer_scaling;
			static const char* labels[] = { "Fractional", "Integer" };
			if (ImGui::Combo("Screen Scaling", &value, labels, IM_ARRAYSIZE(labels))) {
				m_integer_scaling = value != 0;
			}
		} {
			int value = m_screen_rotation;
			static const char* labels[] = { "0 degrees", "90 degrees", "180 degrees", "270 degrees" };
			if (ImGui::Combo("Screen Rotation", &value, labels, IM_ARRAYSIZE(labels))) {
				m_screen_rotation = value;
			}
		} {
			int value = m_utilize_shaders;
			if (ImGui::SliderInt("Shaders Enabled?", &value, 0, 1, "", ImGuiSliderFlags_NoInput)) {
				m_utilize_shaders = value != 0;
			}
		} {
			int value = m_staging_data.enabled ? 1 : 0;
			if (ImGui::SliderInt("Screen Enabled?", &value, 0, 1, "", ImGuiSliderFlags_NoInput)) {
				m_staging_data.enabled = value != 0;
			}
		}});
	}
};

/*==================================================================*/

DisplayDevice::DisplayDevice(std::size_t W, std::size_t H, ImLabel name, std::size_t bpp) noexcept
	: m_context(std::make_unique<DisplayContext>(
		std::clamp(W, std::size_t(1), std::size_t(8192)),
		std::clamp(H, std::size_t(1), std::size_t(8192)),
		name->empty() ? ImLabel("Unnamed Display") : std::move(name),
		std::clamp(bpp, std::size_t(1), std::size_t(6))
	))
{}

DisplayDevice::~DisplayDevice() noexcept = default;

DisplayDevice::DisplayDevice(DisplayDevice&&) noexcept = default;
DisplayDevice& DisplayDevice::operator=(DisplayDevice&&) noexcept = default;

/*==================================================================*/

auto DisplayDevice::swapchain()       noexcept ->       Swapchain& { return m_context->m_swapchain; }
auto DisplayDevice::swapchain() const noexcept -> const Swapchain& { return m_context->m_swapchain; }

auto DisplayDevice::metadata_staging() noexcept -> FramePacket::Metadata& {
	return m_context->m_staging_data;
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

auto DisplayDevice::get_window_label() const noexcept -> ImLabel {
	return *m_context->m_window_label.load(mo::relaxed);
}

void DisplayDevice::set_window_label(std::string_view name) noexcept {
	m_context->m_window_label.store(std::make_shared<ImLabel>(name), mo::relaxed);
}

void DisplayDevice::set_window_state_output(bool* out) noexcept {
	m_context->m_window_state_out = out;
}

void DisplayDevice::set_window_focus_output(bool* out) noexcept {
	m_context->m_window_focus_out = out;
}

void DisplayDevice::set_osd_callable(Callable callable) noexcept {
	m_context->m_osd_callable.store(std::make_shared<Callable>(std::move(callable)), mo::relaxed);
}

/*==================================================================*/

void osd::simple_text_overlay(const std::string& overlay_data) noexcept {
	const auto text_zone = ImGui::CalcTextSize(overlay_data.c_str())
		+ ImGui::GetStyle().WindowPadding * 2.0f;

	ImGui::SetCursorPos(ImGui::GetCursorPos()
		+ (ImGui::GetContentRegionAvail() - text_zone) * ImVec2(0.0f, 1.0f));

	if (ImGui::BeginChild("##text_overlay", text_zone, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav
	)) {
		ImGui::TextUnformatted(overlay_data.c_str());
	}
	ImGui::EndChild();
}

void osd::key_press_indicator(float phase) noexcept {
	const auto text_height = ImGui::GetTextLineHeight();
	const auto size = ImVec2(text_height * 0.8f, text_height);
	const auto widget_zone = size + ImGui::GetStyle().WindowPadding * 2.0f;

	ImGui::SetCursorPos(ImGui::GetCursorPos()
		+ (ImGui::GetContentRegionAvail() - widget_zone) * ImVec2(0.0f, 0.0f));

	if (ImGui::BeginChild("##key_indicator", widget_zone, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav
	)) {
		const auto origin = ImGui::GetCursorScreenPos();
		const auto& style = ImGui::GetStyle();

		{
			const auto pos1 = origin + ImVec2(0.0f, size.y * 0.75f);
			const auto pos2 = pos1 + ImVec2(size.x, size.y * 0.25f);

			ImGui::GetWindowDrawList()->AddRectFilled(pos1, pos2,
				ImGui::GetColorU32(ImGuiCol_Text), 0.0f, ImDrawFlags_None);

			if (style.FrameBorderSize >= 1.0f) {
				ImGui::GetWindowDrawList()->AddRect(pos1, pos2,
					ImGui::GetColorU32(ImGuiCol_Border), 0.0f,
					ImDrawFlags_None, style.FrameBorderSize);
			}
		}

		{
			const auto pos1 = origin + ImVec2(0.0f, size.y * 0.20f * phase);
			const auto pos2 = pos1 + ImVec2(size.x, 0.0f);
			const auto pos3 = pos2 + ImVec2(size.x * -0.5f, size.y * 0.45f);

			ImGui::GetWindowDrawList()->AddTriangleFilled(pos1, pos2, pos3,
				ImGui::GetColorU32(ImGuiCol_Text));

			if (style.FrameBorderSize >= 1.0f) {
				ImGui::GetWindowDrawList()->AddTriangle(pos1, pos2, pos3,
					ImGui::GetColorU32(ImGuiCol_Border), style.FrameBorderSize);
			}
		}

		ImGui::Dummy(widget_zone);
	}
	ImGui::EndChild();
}
