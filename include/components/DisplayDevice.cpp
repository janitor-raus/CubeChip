/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "DisplayDevice.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "BasicVideoSpec.hpp"

#include <imgui.h>

/*==================================================================*/

struct DisplayDevice::DisplayContext {
	using Callable = DisplayDevice::Callable;
	using Metadata = FramePacket::Metadata;

	AtomicBox<Metadata>      m_staging_data;
	AtomSharedPtr<Callable>  m_osd_callable;

	SDL_Renderer* const&     m_renderer_hook;
	SDL_Renderer*            m_live_renderer;
	DisplayDevice::Swapchain m_swapchain;
	SDL_Unique<SDL_Texture>  m_stream_texture;
	SDL_Unique<SDL_Texture>  m_target_texture;

	ez::Frame   m_old_target_size{};
	const bool* m_borderless_view_input = nullptr;

	BoundedParam<0, 0, 3> m_screen_rotation;

	bool m_integer_scaling = false;
	bool m_borderless_view = false;
	bool m_shaders_enabled = false;
	bool m_debugger_enabled = false;

	auto init_stream_texture(int W, int H) const noexcept {
		return BasicVideoSpec::create_stream_texture(m_live_renderer, W, H);
	}

public:
	DisplayContext(std::size_t W, std::size_t H, SDL_Renderer* const& sdl_renderer_ptr) noexcept
		: m_renderer_hook(sdl_renderer_ptr)
		, m_live_renderer(nullptr)
		, m_swapchain(int(W), int(H))
		, m_staging_data(std::make_shared<Metadata>(int(W), int(H)))
	{
		assert((m_staging_data.view()->get_base_frame().area() == (W * H))
			&& "Display W/H sizes are beyond expected bounds, clamping!");
	}

private:
	class DisplayLayout {
		const Metadata& m_metadata_ref;

	public:
		ImVec2 origin_point{};
		ImVec2 avail_region{};

		ImVec2 texture_region{};
		ImVec2 margins_region{};

		ez::Rect dar_viewport{}; // display aspect ratio corrected viewport

		f32 base_ratio{};

		explicit DisplayLayout(const Metadata& metadata, const DisplayContext* ctx) noexcept
			: m_metadata_ref(metadata)
		{
			const auto cur_viewport = metadata.get_viewport();
			dar_viewport = (*ctx->m_screen_rotation & 1)
				? ez::Rect(cur_viewport.y, cur_viewport.x,
					s32(cur_viewport.h * metadata.pixel_ratio), cur_viewport.w)
				: ez::Rect(cur_viewport.x, cur_viewport.y,
					s32(cur_viewport.w * metadata.pixel_ratio), cur_viewport.h);

		// calc borders/margins width (if borderless_view is disabled)
			const auto borders_width = ctx->m_borderless_view
				? 0.0f : 2.0f * f32((metadata.border_width + 1) / 2);
			const auto margins_width = ctx->m_borderless_view
				? 0.0f : 2.0f * f32(metadata.inner_margin) + borders_width;

			avail_region = ImGui::GetContentRegionAvail();
			origin_point = ImGui::GetCursorPos();

		// calc AR value
			base_ratio = std::min(
				(avail_region.x - borders_width - margins_width) / dar_viewport.w,
				(avail_region.y - borders_width - margins_width) / dar_viewport.h
			);

		// calc drawing areas fixed by AR for consistency
			texture_region = ImVec2(f32(dar_viewport.w), f32(dar_viewport.h))
				* std::max(ctx->m_integer_scaling ? std::floor(base_ratio)
					: base_ratio, f32(metadata.minimum_zoom));
			margins_region = texture_region + ImVec2(margins_width, margins_width);
		}

		auto operator->() const noexcept { return &m_metadata_ref; }
	};

	void sync_renderer_change() noexcept {
		if (m_renderer_hook == m_live_renderer) { return; }

		if (m_renderer_hook != nullptr) {
			const auto frame = m_staging_data.view()->get_base_frame();
			m_stream_texture.reset(BasicVideoSpec::create_stream_texture(
				m_live_renderer = m_renderer_hook, frame.w, frame.h));
		} else {
			m_stream_texture.reset();
			m_live_renderer = nullptr;
		}
		m_target_texture.reset();
	}

private:
	void render_texture_region(const DisplayLayout& layout_data) noexcept {
		if (layout_data->enabled) {
			const auto target_AR = std::max(std::floor(layout_data.base_ratio),
				f32(layout_data->minimum_zoom));

			const auto new_target_size = ez::Frame(
				s32(std::ceil(layout_data.dar_viewport.w * target_AR)),
				s32(std::ceil(layout_data.dar_viewport.h * target_AR))
			);

			if (!m_target_texture || new_target_size != m_old_target_size) {
				m_target_texture.reset(BasicVideoSpec::create_target_texture(
					m_live_renderer, new_target_size.w, new_target_size.h, true));
				m_old_target_size = new_target_size;
			}

			BasicVideoSpec::write_stream_texture(m_live_renderer,
				m_target_texture, m_stream_texture);

			ImGui::SetCursorPos(layout_data.origin_point + ImGui::floor(
				(layout_data.avail_region - layout_data.margins_region) * 0.5f));

			ImGui::DrawRectFilled(layout_data.margins_region, !m_borderless_view
				* ImGui::GetStyle().FrameRounding, layout_data->texture_tint.XBGR());

			const auto cur_viewport = layout_data->get_viewport();
			const auto uv0 = ImVec2(
				cur_viewport.x / f32(layout_data->get_base_frame().w),
				cur_viewport.y / f32(layout_data->get_base_frame().h)
			);
			const auto uv1 = ImVec2(
				(cur_viewport.x + cur_viewport.w) / f32(layout_data->get_base_frame().w),
				(cur_viewport.y + cur_viewport.h) / f32(layout_data->get_base_frame().h)
			);

			ImGui::SetCursorPos(layout_data.origin_point + ImGui::floor(
				(layout_data.avail_region - layout_data.texture_region) * 0.5f));

			ImGui::DrawRotatedImage(m_target_texture, layout_data.texture_region, *m_screen_rotation,
				uv0, uv1, RGBA(0xFF, 0xFF, 0xFF, layout_data->texture_tint.A).ABGR());
		}
	}
	void render_borders_region(const DisplayLayout& layout_data) const noexcept {
		if (!m_borderless_view && layout_data->border_width != 0) {
			ImGui::SetCursorPos(layout_data.origin_point + ImGui::floor(
				(layout_data.avail_region - layout_data.margins_region) * 0.5f));

			ImGui::DrawRect(layout_data.margins_region, f32(layout_data->border_width),
				ImGui::GetStyle().FrameRounding, layout_data->border_color.XBGR());
		}
	}
	void render_osd_callable(const DisplayLayout& layout_data) const noexcept {
		if (!m_debugger_enabled) {
			auto osd_callable = m_osd_callable.load(std::memory_order::relaxed);
			if (osd_callable) {
				using namespace ImGui;
				SetCursorPos(layout_data.origin_point);
				(*osd_callable)();
			}
		}
	}
	void render_debug_region(const DisplayLayout& layout_data) const noexcept {
		if (m_debugger_enabled) {
			using namespace ImGui;
			SetCursorPos(layout_data.origin_point);

			if (BeginTable("##debug_table", 2, ImGuiTableFlags_RowBg
				| ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody
			)) {
				static auto row = [](const char* label, auto fmt, auto... args) {
					TableNextRow();
					TableSetColumnIndex(0);
					TextUnformatted(label);
					TableSetColumnIndex(1);
					Text(fmt, args...);
				};
				static auto row_separate = []() {
					TableNextRow();
					TableSetColumnIndex(0);
					Dummy(ImVec2(0.0f, GetTextLineHeight() * 0.3f));
				};

				row("Viewport (w,h):", "%d×%d", layout_data.dar_viewport.w, layout_data.dar_viewport.h);
				row("Viewport (x,y):", "%d,%d", layout_data.dar_viewport.x, layout_data.dar_viewport.y);
				row_separate();
				row("Avail region:",   "%.0f×%.0f", layout_data.avail_region.x, layout_data.avail_region.y);
				row("Margins region:", "%.0f×%.0f", layout_data.margins_region.x, layout_data.margins_region.y);
				row("Texture region:", "%.0f×%.0f", layout_data.texture_region.x, layout_data.texture_region.y);
				row_separate();
				row("Aspect ratio:", "%.3f|%.3f (min)", layout_data.base_ratio,
					std::max(std::floor(layout_data.base_ratio), f32(layout_data->minimum_zoom)));
				row("Texture scaled to:", "%d×%d", m_old_target_size.w, m_old_target_size.h);

				EndTable();
			}
		}
	}

public:
	void render_display() noexcept {
		using namespace ImGui;

		PushStyleColor(ImGuiCol_ChildBg, 0);
		PushID(this);
		const bool visible = BeginChild("##display_child");
		PopStyleColor();

		if (!visible) { EndChild(); PopID(); return; }

		sync_renderer_change();

		m_swapchain.present([&](auto frame) noexcept {
			if constexpr (frame.dirty) {
				BasicVideoSpec::write_stream_texture(m_live_renderer,
					m_stream_texture, frame.buffer.data());
			}

			m_borderless_view = m_borderless_view_input
				? *m_borderless_view_input : m_borderless_view;

			const auto metadata_view = m_staging_data.view();
			const auto layout_data = DisplayLayout(metadata_view->debug_mode
				? *metadata_view : frame.buffer.metadata, this);

			render_texture_region(layout_data);
			render_borders_region(layout_data);

			render_osd_callable(layout_data);
			render_debug_region(layout_data);

			SetCursorPos(layout_data.origin_point);
			Dummy(layout_data.avail_region); // advance layout
		});

		EndChild();
		PopID();
	}
	void render_settings_menu() noexcept {
		if (!ImGui::BeginMenu("Display Settings")) { return; }
		m_staging_data.edit([&](auto& meta) noexcept {
			ImGui::SliderInt("Border Width", &*meta.border_width,
				meta.border_width.min,
				meta.border_width.max,
				"%d", ImGuiSliderFlags_AlwaysClamp
			);

			ImGui::SliderInt("Inner Margin", &*meta.inner_margin,
				meta.inner_margin.min,
				meta.inner_margin.max,
				"%d", ImGuiSliderFlags_AlwaysClamp
			);

			const auto border_color = meta.border_color;
			float border_color_c[] = {
				border_color.R / 255.0f, border_color.G / 255.0f,
				border_color.B / 255.0f, 1.0f
			};
			if (ImGui::ColorEdit3("Border Color", border_color_c)) {
				meta.border_color = RGBA(
					u8(border_color_c[0] * 255.0f), u8(border_color_c[1] * 255.0f),
					u8(border_color_c[2] * 255.0f), u8(border_color_c[3] * 255.0f)
				);
			}

			const auto texture_tint = meta.texture_tint;
			float texture_tint_c[] = {
				texture_tint.R / 255.0f, texture_tint.G / 255.0f,
				texture_tint.B / 255.0f, texture_tint.A / 255.0f
			};
			if (ImGui::ColorEdit4("Texture Tint", texture_tint_c, ImGuiColorEditFlags_AlphaPreviewHalf)) {
				meta.texture_tint = RGBA(
					u8(texture_tint_c[0] * 255.0f), u8(texture_tint_c[1] * 255.0f),
					u8(texture_tint_c[2] * 255.0f), u8(texture_tint_c[3] * 255.0f)
				);
			}

			ImGui::SliderInt("Minimum Zoom", &*meta.minimum_zoom,
				meta.minimum_zoom.min,
				meta.minimum_zoom.max,
				"%d", ImGuiSliderFlags_AlwaysClamp
			);

			ImGui::SliderFloat("Pixel Ratio", &*meta.pixel_ratio,
				meta.pixel_ratio.min,
				meta.pixel_ratio.max,
				"%.2f", ImGuiSliderFlags_AlwaysClamp
			);

			int integer_scaling = m_integer_scaling;
			static const char* scaling_labels[] = { "Fractional", "Integer" };
			if (ImGui::Combo("Screen Scaling", &integer_scaling, scaling_labels, 2)) {
				m_integer_scaling = integer_scaling != 0;
			}

			static const char* rotation_labels[] = { "0 degrees", "90 degrees", "180 degrees", "270 degrees" };
			ImGui::Combo("Screen Rotation", &*m_screen_rotation, rotation_labels, 4);

			int enable_screen = meta.enabled ? 1 : 0;
			if (ImGui::SliderInt("Screen Enabled?", &enable_screen,
				0, 1, "", ImGuiSliderFlags_NoInput
			)) {
				meta.enabled = enable_screen != 0;
			}

			ImGui::BeginDisabled(m_borderless_view_input);
			int borderless_view = m_borderless_view ? 1 : 0;
			if (ImGui::SliderInt("Borderless View?", &borderless_view,
				0, 1, "", ImGuiSliderFlags_NoInput
			)) {
				m_borderless_view = borderless_view != 0;
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(true);
			int shaders_enabled = m_shaders_enabled;
			if (ImGui::SliderInt("Shaders Enabled?", &shaders_enabled,
				0, 1, "", ImGuiSliderFlags_NoInput
			)) {
				m_shaders_enabled = shaders_enabled != 0;
			}
			ImGui::EndDisabled();

			int debugger_enabled = m_debugger_enabled ? 1 : 0;
			if (ImGui::SliderInt("Debugger Enabled?", &debugger_enabled,
				0, 1, "", ImGuiSliderFlags_NoInput
			)) {
				m_debugger_enabled = debugger_enabled != 0;
			}

		});
		ImGui::EndMenu();
	}
};

/*==================================================================*/

DisplayDevice::DisplayDevice(std::size_t W, std::size_t H,
	SDL_Renderer* const& sdl_renderer_ptr) noexcept
	: m_context(std::make_unique<DisplayContext>(
		std::clamp<std::size_t>(W, 1u, 8192u),
		std::clamp<std::size_t>(H, 1u, 8192u),
		sdl_renderer_ptr
	))
{}

DisplayDevice::~DisplayDevice() noexcept = default;

DisplayDevice::DisplayDevice(DisplayDevice&&) noexcept = default;
DisplayDevice& DisplayDevice::operator=(DisplayDevice&&) noexcept = default;

/*==================================================================*/

auto DisplayDevice::swapchain() /***/ noexcept -> /***/ Swapchain& { return m_context->m_swapchain; }
auto DisplayDevice::swapchain() const noexcept -> const Swapchain& { return m_context->m_swapchain; }

auto DisplayDevice::metadata() /***/ noexcept -> /***/ AtomicBox<Metadata>& { return m_context->m_staging_data; }
auto DisplayDevice::metadata() const noexcept -> const AtomicBox<Metadata>& { return m_context->m_staging_data; }

/*==================================================================*/

int  DisplayDevice::get_screen_rotation() const noexcept {
	return m_context->m_screen_rotation;
}

void DisplayDevice::set_screen_rotation(int rotation) noexcept {
	m_context->m_screen_rotation = rotation & 3;
}

bool DisplayDevice::has_integer_scaling() const noexcept {
	return m_context->m_integer_scaling;
}

void DisplayDevice::set_integer_scaling(bool enable) noexcept {
	m_context->m_integer_scaling = enable;
}

bool DisplayDevice::has_shaders_enabled() const noexcept {
	return m_context->m_shaders_enabled;
}

void DisplayDevice::set_shaders_enabled(bool enable) noexcept {
	m_context->m_shaders_enabled = enable;
}

void DisplayDevice::set_borderless_view_input(const bool* input) noexcept {
	m_context->m_borderless_view_input = input;
}

bool DisplayDevice::has_borderless_view() const noexcept {
	return m_context->m_borderless_view;
}

void DisplayDevice::set_borderless_view(bool enable) noexcept {
	m_context->m_borderless_view = enable;
}

void DisplayDevice::set_osd_callable(Callable callable) noexcept {
	m_context->m_osd_callable.store(std::make_shared<Callable>(
		std::move(callable)), std::memory_order::relaxed);
}

void DisplayDevice::render_display() noexcept {
	m_context->render_display();
}

void DisplayDevice::render_settings_menu() noexcept {
	m_context->render_settings_menu();
}
