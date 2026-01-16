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

#include <imgui.h>

/*==================================================================*/

static auto renderer() noexcept { return FrontendInterface::get_current_renderer(); }

struct DisplayDevice::DisplayContext {
	using Callable = DisplayDevice::Callable;

	AtomSharedPtr<std::string> m_display_name;
	AtomSharedPtr<Callable>    m_osd_callable;
	FrontendInterface::Hook    m_render_hook;
	SDL_Unique<SDL_Texture>    m_stream_texture;
	SDL_Unique<SDL_Texture>    m_target_texture;
	DisplayDevice::Swapchain   m_swapchain;

	std::atomic<int>  m_screen_rotation{};
	std::atomic<bool> m_integer_scaling{};
	std::atomic<bool> m_utilize_shaders{};

	bool m_menubar_hidden = true;
	bool m_display_debug = false;

	ez::Frame m_old_target_size{};

	DisplayContext(std::size_t W, std::size_t H, const char* name, std::size_t bpp) noexcept
		: m_display_name(std::make_shared<std::string>(name))
		, m_osd_callable(nullptr)
		, m_render_hook(FrontendInterface::register_window(
			[&]() noexcept { render_display_window(); }))
		, m_stream_texture(BasicVideoSpec::create_stream_texture(
			::renderer(), static_cast<int>(W), static_cast<int>(H)))
		, m_swapchain(bpp, static_cast<int>(W), static_cast<int>(H))
	{}

private:
	void render_display_window() noexcept {
		m_swapchain.present([&](auto frame) noexcept {
			const auto& metadata = frame.buffer.metadata;

			BasicVideoSpec::write_stream_texture(::renderer(),
				m_stream_texture, frame.buffer.data());

			const auto rotation = m_screen_rotation.load(mo::relaxed);
			const auto integer  = m_integer_scaling.load(mo::relaxed);

			const auto px_ratio = float(metadata.get_pixel_ratio());
			const auto margin   = float(metadata.get_inner_margin());
			const auto min_zoom = float(metadata.get_texture_zoom());

			const auto cur_viewport = metadata.get_flipped_viewport_if(rotation & 1);
			/***/ auto dar_viewport = cur_viewport; dar_viewport.w = int(dar_viewport.w * px_ratio);

			const auto border_width = metadata.get_border_width() / 2 * 2;
			const auto borders_vec2 = ImVec2(float(border_width), float(border_width));
			const auto padding_vec2 = ImVec2(margin * 2, margin * 2) + borders_vec2;

			ImGui::DockNextWindowTo(FrontendInterface::get_main_dockspace_id(), true);
			ImGui::SetNextWindowMinClientSize(ImVec2(float(dar_viewport.w), float(dar_viewport.h))
				* min_zoom + padding_vec2 + borders_vec2);

			const auto window_name = m_display_name.load(mo::relaxed);
			if (ImGui::Begin(window_name->c_str(), nullptr, ImGuiWindowFlags_NoCollapse
				| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
				| (m_menubar_hidden ? ImGuiWindowFlags_None : ImGuiWindowFlags_MenuBar)
			)) {
				FrontendInterface::call_autohide_menubar(
					window_name->c_str(), m_menubar_hidden);

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
					* std::max(integer ? std::floor(base_AR) : base_AR, min_zoom);
				const auto borders_area = texture_area + padding_vec2;

			// calc target texture size for integer nn -> fractional linear scaling
				const auto target_AR = std::max(std::floor(base_AR), min_zoom);

				const auto new_target_size = ez::Frame(
					int(dar_viewport.w * target_AR), int(dar_viewport.h * target_AR));

				if (metadata.enabled) {
					if (new_target_size != m_old_target_size) {
						m_target_texture.reset(BasicVideoSpec::create_target_texture(
							::renderer(), new_target_size.w, new_target_size.h, true));
						m_old_target_size = new_target_size;
					}

					BasicVideoSpec::write_stream_texture(
						::renderer(), m_target_texture, m_stream_texture);

					ImGui::SetCursorPos(origin_point + ImGui::floor(
						(display_zone - borders_area) * 0.5f));

					ImGui::DrawRectFilled(borders_area,
						margin, metadata.get_texture_tint().XBGR());

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

					ImGui::DrawRotatedImage(m_target_texture, texture_area, rotation, uv0, uv1,
						RGBA(0xFF, 0xFF, 0xFF, metadata.get_texture_tint().A).ABGR());
				}

				if (metadata.get_border_width() >= 1.0f) {
					ImGui::SetCursorPos(origin_point + ImGui::floor(
						(display_zone - borders_area) * 0.5f));

					ImGui::DrawRect(borders_area,
						metadata.get_border_width(), margin,
						metadata.get_border_color().XBGR());
				}

				// apply dummy just in case
				ImGui::SetCursorPos(origin_point);
				ImGui::Dummy(display_zone);
				ImGui::SetCursorPos(origin_point);

				auto osd_callable = m_osd_callable.load(mo::relaxed);
				if (osd_callable) { (*osd_callable)(); }

				if (m_display_debug) {
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
			}
			ImGui::End();
		});
	}
};

/*==================================================================*/

DisplayDevice::DisplayDevice(std::size_t W, std::size_t H, const char* name, std::size_t bpp, int) noexcept
	: m_context(std::make_unique<DisplayContext>(W, H, name, bpp))
	, metadata_staging(static_cast<int>(W), static_cast<int>(H))
{
#if !defined(NDEBUG) || defined(DEBUG)
	bind_debug_menu_hooks();
#endif
}

DisplayDevice::DisplayDevice(std::size_t W, std::size_t H, const char* name, std::size_t bpp) noexcept
	: DisplayDevice(
		std::clamp(W, std::size_t(1), std::size_t(8192)),
		std::clamp(H, std::size_t(1), std::size_t(8192)),
		name ? name : "Unnamed Display",
		std::clamp(bpp, std::size_t(1), std::size_t(6)), 0
	)
{
	if (metadata_staging.get_base_frame().area() != (W * H)) {
		blog.newEntry<BLOG::WRN>("Display W/H out of size bounds, clamping!");
	}
}

DisplayDevice::~DisplayDevice() noexcept = default;

#if !defined(NDEBUG) || defined(DEBUG)
DisplayDevice::DisplayDevice(DisplayDevice&& other) noexcept
	: m_context(std::move(other.m_context))
	, metadata_staging(std::move(other.metadata_staging))
{
	other.free_debug_menu_hooks();
	this->bind_debug_menu_hooks();
}

DisplayDevice& DisplayDevice::operator=(DisplayDevice&& other) noexcept {
	if (this != &other) {
		m_context = std::move(other.m_context);
		metadata_staging = std::move(other.metadata_staging);

		other.free_debug_menu_hooks();
		this->bind_debug_menu_hooks();
	}
	return *this;
}
#else
DisplayDevice::DisplayDevice(DisplayDevice&&) noexcept = default;
DisplayDevice& DisplayDevice::operator=(DisplayDevice&&) noexcept = default;
#endif

/*==================================================================*/

auto DisplayDevice::swapchain()       noexcept ->       Swapchain& { return m_context->m_swapchain; }
auto DisplayDevice::swapchain() const noexcept -> const Swapchain& { return m_context->m_swapchain; }

/*==================================================================*/

int  DisplayDevice::get_screen_rotation() const noexcept {
	return m_context->m_screen_rotation.load(mo::relaxed);
}

void DisplayDevice::set_screen_rotation(int rotation) noexcept {
	m_context->m_screen_rotation.store(rotation & 3, mo::relaxed);
}

bool DisplayDevice::get_integer_scaling() const noexcept {
	return m_context->m_integer_scaling.load(mo::relaxed);
}

void DisplayDevice::set_integer_scaling(bool enable) noexcept {
	m_context->m_integer_scaling.store(enable, mo::relaxed);
}

bool DisplayDevice::get_utilize_shaders() const noexcept {
	return m_context->m_utilize_shaders.load(mo::relaxed);
}

void DisplayDevice::set_utilize_shaders(bool enable) noexcept {
	m_context->m_utilize_shaders.store(enable, mo::relaxed);
}

auto DisplayDevice::get_display_name() const noexcept -> std::string {
	return *m_context->m_display_name.load(mo::relaxed);
}

void DisplayDevice::set_display_name(std::string_view name) noexcept {
	m_context->m_display_name.store(std::make_shared<std::string>(name), mo::relaxed);
}

void DisplayDevice::set_osd_callable(Callable callable) noexcept {
	m_context->m_osd_callable.store(std::make_shared<Callable>(std::move(callable)), mo::relaxed);
}

#if !defined(NDEBUG) || defined(DEBUG)
void DisplayDevice::bind_debug_menu_hooks() noexcept {
	const auto window_label = ImLabel(get_display_name());

	m_debug_border_width = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		int value = metadata_staging.get_border_width();
		if (ImGui::SliderInt(" Border Width", &value, 0, 32, "%d", ImGuiSliderFlags_AlwaysClamp)) {
			metadata_staging.set_border_width(value);
		}
	});

	m_debug_inner_margin = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		int value = metadata_staging.get_inner_margin();
		if (ImGui::SliderInt(" Inner Margin", &value, 0, 32, "%d", ImGuiSliderFlags_AlwaysClamp)) {
			metadata_staging.set_inner_margin(value);
		}
	});

	m_debug_border_color = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		const auto temp = metadata_staging.get_border_color();
		float color[3] = { (temp.R / 255.0f), (temp.G / 255.0f), (temp.B / 255.0f) };
		if (ImGui::ColorEdit3(" Border Color", color)) {
			metadata_staging.set_border_color(RGBA(
				ez::u8(color[0] * 255.0f), ez::u8(color[1] * 255.0f),
				ez::u8(color[2] * 255.0f), 255
			));
		}
	});

	m_debug_texture_tint = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		const auto temp = metadata_staging.get_texture_tint();
		float color[4] = { (temp.R / 255.0f), (temp.G / 255.0f), (temp.B / 255.0f), (temp.A / 255.0f) };
		if (ImGui::ColorEdit4(" Texture Tint", color)) {
			metadata_staging.set_texture_tint(RGBA(
				ez::u8(color[0] * 255.0f), ez::u8(color[1] * 255.0f),
				ez::u8(color[2] * 255.0f), ez::u8(color[3] * 255.0f)
			));
		}
	});

	m_debug_texture_zoom = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		int value = metadata_staging.get_texture_zoom();
		if (ImGui::SliderInt(" Texture Zoom", &value, 1, 32, "%d", ImGuiSliderFlags_AlwaysClamp)) {
			metadata_staging.set_texture_zoom(value);
		}
	});

	m_debug_pixel_ratio = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		float value = metadata_staging.get_pixel_ratio();
		if (ImGui::SliderFloat(" Pixel Ratio", &value, 0.1f, 4.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
			metadata_staging.set_pixel_ratio(value);
		}
	});

	m_debug_linear_scaling = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		int a = !get_integer_scaling();
		if (ImGui::SliderInt(" Linear Scaling?", &a, 0, 1, "", ImGuiSliderFlags_NoInput)) {
			set_integer_scaling(a == 0);
		}
	});

	m_debug_screen_enabled = FrontendInterface::register_menu(window_label,
	{ 0, "Debug" }, [&]() noexcept {
		int a = metadata_staging.enabled ? 1 : 0;
		if (ImGui::SliderInt(" Screen Enabled?", &a, 0, 1, "", ImGuiSliderFlags_NoInput)) {
			metadata_staging.enabled = (a != 0);
		}
	});
}

void DisplayDevice::free_debug_menu_hooks() noexcept {
	m_debug_border_width.reset();
	m_debug_inner_margin.reset();
	m_debug_border_color.reset();
	m_debug_texture_tint.reset();
	m_debug_texture_zoom.reset();
	m_debug_pixel_ratio.reset();
	m_debug_linear_scaling.reset();
	m_debug_screen_enabled.reset();
}
#endif

/*==================================================================*/

void osd::simple_stat_overlay(const std::string& overlay_data) noexcept {
	const ImVec2 text_zone = ImGui::CalcTextSize(overlay_data.c_str())
		+ ImGui::GetStyle().WindowPadding * 2.0f;

	ImGui::SetCursorPos(ImGui::GetCursorPos()
		+ (ImGui::GetContentRegionAvail() - text_zone) * ImVec2(0.0f, 1.0f));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBA(20, 20, 20, 80).ABGR());
	if (ImGui::BeginChild("##osd_overlay", text_zone, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav
	)) {
		ImGui::TextUnformatted(overlay_data.c_str());
		ImGui::Dummy(ImGui::GetStyle().WindowPadding);
		ImGui::EndChild();
	}
	ImGui::PopStyleColor();
}
