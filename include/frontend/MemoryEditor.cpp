/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "MemoryEditor.hpp"

#include <cstddef>
#include <cstring>
#include <array>
#include <charconv>
#include <bit>
#include <span>
#include <cstdio>
#include <algorithm>

#include <imgui.h>

/*==================================================================*/

void MemoryEditor::recalculate_all_sizes() {
	using namespace ImGui;

	const auto max_address = internals.base_display_address + internals.memory_size - 1;
	auto auto_address_digits = 0u;
	for (auto n = max_address; n > 0u; n >>= 4) { ++auto_address_digits; }
	// Ensure we use the biggest amount of the two -- we don't want less than what's
	// needed to represent the full memory range we are iterating over.
	sizes.address_digit_count = std::max(auto_address_digits, settings.address_digit_count);

	sizes.glyph_width = CalcTextSize("F").x + 1.0f;// We assume the font is mono-space
	sizes.hex_cell_width    = std::floor(sizes.glyph_width * 2.5f);  // "FF " we include trailing space in the width to easily catch clicks everywhere
	sizes.col_group_spacing = std::floor(sizes.hex_cell_width * 0.25f); // Every column_group_size columns we add a bit of extra spacing

	sizes.hex_offset_min = (sizes.address_digit_count + 2) * sizes.glyph_width;
	sizes.hex_offset_max = sizes.hex_offset_min + (sizes.hex_cell_width * settings.column_count);

	sizes.ascii_offset_min = sizes.ascii_offset_max = sizes.hex_offset_max;
	if (settings.toggle_ascii_view) {
		sizes.ascii_offset_min = sizes.hex_offset_max + sizes.glyph_width * 1;
		if (settings.column_group_size > 0) {
			settings.column_group_size = std::bit_floor(*settings.column_group_size);
			sizes.ascii_offset_min += sizes.col_group_spacing * float(
				(settings.column_count + settings.column_group_size - 1) / settings.column_group_size);
		}
		sizes.ascii_offset_max = sizes.ascii_offset_min + settings.column_count * sizes.glyph_width;
	}

	if (settings.cell_emphasis_color == RGBA::Transparent) {
		settings.cell_emphasis_color = GetColorU32(ImGuiCol_TabSelected);
	}
}

auto MemoryEditor::get_highlight_type(std::size_t address) noexcept -> HighlightInfo {
	auto ret_highlight = HighlightInfo();

	if (address >= internals.memory_size) {
		return ret_highlight;
	}

	const auto preview_data_type_size = settings.show_data_preview
		? get_data_type_size(internals.preview_data_type) : 1;

	if (internals.highlight_max != c_max_addr
		&& address >= internals.highlight_min
		&& address <  internals.highlight_max
	) {
		ret_highlight.type |= HighlightType::Follow;
	}

	if (callbacks.highlight &&
		callbacks.highlight(internals.memory_data, address)
	) {
		ret_highlight.type |= HighlightType::Callback;
	}

	if (internals.data_preview_address != c_max_addr
		&& address >= internals.data_preview_address
		&& address <  internals.data_preview_address + preview_data_type_size
	) {
		ret_highlight.type |= HighlightType::Preview;
	}

	if (ret_highlight.type == HighlightType::None) {
		if (callbacks.bg_color) {
			ret_highlight.type  = HighlightType::BgColor;
			ret_highlight.color = callbacks.bg_color(internals.memory_data, address);
		}
	} else {
		ret_highlight.color = settings.cell_emphasis_color;
	}

	return ret_highlight;
};

void MemoryEditor::read_bytes(
	std::span<const u8> src,
	std::span</***/ u8> dst,
	std::size_t addr
) const {
	if (addr >= src.size()) { return; }

	const auto count = std::min(dst.size(), src.size() - addr);

	if (callbacks.read) {
		for (std::size_t i = 0; i < count; ++i) {
			dst[i] = callbacks.read(src.data(), addr + i);
		}
	} else {
		std::memcpy(dst.data(), src.data() + addr, count);
	}
}

void MemoryEditor::endian_aware_copy(void* dst, const void* src, size_t size) const {
	const auto* src_bytes = static_cast<const std::byte*>(src);
	/***/ auto* dst_bytes = static_cast</***/ std::byte*>(dst);

	const bool need_swap =
		(std::endian::native == std::endian::little &&  internals.preview_endianness) ||
		(std::endian::native == std::endian::big    && !internals.preview_endianness);

	if (need_swap) {
		std::reverse_copy(src_bytes, src_bytes + size, dst_bytes);
	} else {
		std::copy(src_bytes, src_bytes + size, dst_bytes);
	}
}

void MemoryEditor::format_value(
	DataType type, std::span<char> output,
	std::span<const u8> bytes, DataFormat fmt
) {
	switch (type) {
		case DataType::S8:  typed_formatter(output, load_value< s8>(bytes), fmt); break;
		case DataType::U8:  typed_formatter(output, load_value< u8>(bytes), fmt); break;
		case DataType::S16: typed_formatter(output, load_value<s16>(bytes), fmt); break;
		case DataType::U16: typed_formatter(output, load_value<u16>(bytes), fmt); break;
		case DataType::S32: typed_formatter(output, load_value<s32>(bytes), fmt); break;
		case DataType::U32: typed_formatter(output, load_value<u32>(bytes), fmt); break;
		case DataType::S64: typed_formatter(output, load_value<s64>(bytes), fmt); break;
		case DataType::U64: typed_formatter(output, load_value<u64>(bytes), fmt); break;
		case DataType::F32: typed_formatter(output, load_value<f32>(bytes), fmt); break;
		case DataType::F64: typed_formatter(output, load_value<f64>(bytes), fmt); break;
		default: output[0] = '\0'; break; // safety measure
	}
}

void MemoryEditor::render_memory_editor() {
	using namespace ImGui;

	if (!internals.memory_data || internals.memory_size == 0) {
		TextUnformatted("No memory to display.", GetColorU32(ImGuiCol_TextDisabled));
		return;
	}
	recalculate_all_sizes();

	// We begin into our scrolling region with the 'ImGuiWindowFlags_NoMove' in order to prevent click from moving the window.
	// This is used as a facility since our main click detection code doesn't assign an ActiveId so the click would normally be caught as a window-move.

	auto footer_area_height = settings.footer_area_height;
	if (settings.show_options_menu) {
		footer_area_height += GetStyle().ItemSpacing.y * 2 + GetFrameHeightWithSpacing();
	}
	if (settings.show_data_preview) {
		footer_area_height += GetStyle().ItemSpacing.y * 2 + GetFrameHeightWithSpacing()
			+ GetTextLineHeightWithSpacing() * 3;
	}

	const auto contents_pos_start = GetCursorScreenPos();

	BeginChild("##scrolling", ImVec2(-FLT_MIN, -footer_area_height),
		ImGuiChildFlags_None, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
	auto* draw_list = GetWindowDrawList();

	PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2());

	// We are not really using the clipper API correctly here, because we
	// rely on visible_start_addr/visible_end_addr for our scrolling function.
	const auto line_height = GetTextLineHeight();
	const auto region_size = GetContentRegionAvail();
	const auto total_lines = (internals.memory_size + settings.column_count - 1) / settings.column_count;

	ImGuiListClipper clipper;
	clipper.Begin(s32(total_lines), line_height);

	if (internals.data_editing_address >= internals.memory_size) {
		internals.data_editing_address = c_max_addr;
	}
	if (internals.data_preview_address >= internals.memory_size) {
		internals.data_preview_address = c_max_addr;
	}

	const auto rounding = GetStyle().FrameRounding;

	bool advance_editing_cursor = false;
	bool init_highlighting_once = true;
	auto data_editing_addr_next = c_max_addr;

	// Input: move selection cursor with the arrow keys, but only apply on the next
	//        frame so scrolling with be synchronized (as we currently can't change
	//        the scrolling while the window is being rendered)
	if (internals.data_editing_address != c_max_addr) {
		if (IsKeyPressed(ImGuiKey_UpArrow)
			&& std::ptrdiff_t(internals.data_editing_address) >= std::ptrdiff_t(settings.column_count)
		) {
			data_editing_addr_next = internals.data_editing_address - settings.column_count;
		}
		else if (IsKeyPressed(ImGuiKey_DownArrow)
			&& std::ptrdiff_t(internals.data_editing_address) < std::ptrdiff_t(internals.memory_size - settings.column_count)
		) {
			data_editing_addr_next = internals.data_editing_address + settings.column_count;
		}
		else if (IsKeyPressed(ImGuiKey_LeftArrow)
			&& std::ptrdiff_t(internals.data_editing_address) > std::ptrdiff_t()
		) {
			data_editing_addr_next = internals.data_editing_address - 1;
		}
		else if (IsKeyPressed(ImGuiKey_RightArrow)
			&& std::ptrdiff_t(internals.data_editing_address) < std::ptrdiff_t(internals.memory_size - 1)
		) {
			data_editing_addr_next = internals.data_editing_address + 1;
		}
	}

	// Separators: draw between the address, hex view and the ascii view (if enabled)
	const auto window_pos = GetWindowPos();

	if (true) {
		const auto offset_x = window_pos.x
			+ sizes.hex_offset_min - sizes.glyph_width
			- sizes.col_group_spacing * 0.5f;

		draw_list->AddLine(
			ImVec2(offset_x, window_pos.y),
			ImVec2(offset_x, window_pos.y + 9999),
			GetColorU32(ImGuiCol_Border)
		);
	}

	if (settings.toggle_ascii_view) {
		const auto offset_x = window_pos.x +
			sizes.ascii_offset_min - sizes.glyph_width;

		draw_list->AddLine(
			ImVec2(offset_x, window_pos.y),
			ImVec2(offset_x, window_pos.y + 9999),
			GetColorU32(ImGuiCol_Border)
		);
	}

	const auto cell_text_color = GetColorU32(ImGuiCol_Text);
	const auto cell_text_color_off = settings.toggle_grey_zeroes
		? GetColorU32(ImGuiCol_TextDisabled) : cell_text_color;

	const char* format_data       = settings.toggle_capital_hex ? "%0*zX" : "%0*zx";
	const char* format_byte       = settings.toggle_capital_hex ? "%02X"  : "%02x";
	const char* format_byte_space = settings.toggle_capital_hex ? "%02X " : "%02x ";

	auto prev_highlight_type = HighlightInfo();
	auto curr_highlight_type = HighlightInfo();

	while (clipper.Step()) {
		// Line Render: only render lines visible in the current clip region
		for (auto line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; ++line_i) {
			// Segment: display the address offset of each memory line
			const auto line_origin = GetCursorScreenPos();
			draw_list->AddRectFilled(line_origin, line_origin + ImVec2(9999, line_height),
				GetColorU32((line_i & 1) ? ImGuiCol_TableRowBgAlt : ImGuiCol_TableRowBg)
			);

			auto address = std::size_t(line_i) * settings.column_count;
			Text(format_data, sizes.address_digit_count, internals.base_display_address + address);

			if (init_highlighting_once) {
				prev_highlight_type = get_highlight_type(address - 1);
				curr_highlight_type = get_highlight_type(address + 0);
				init_highlighting_once = false;
			}

			// Segment: display each byte in hexadecimal format
			for (auto n = 0u; n < settings.column_count && address < internals.memory_size; ++n, ++address) {
				SameLine(sizes.hex_offset_min + sizes.hex_cell_width * n + sizes.col_group_spacing
					* float(settings.column_group_size ? (n / settings.column_group_size) : 0));

				const auto next_highlight_type = get_highlight_type(address + 1);

				if (curr_highlight_type.color.A != 0) {
					const bool same_as_prev = (prev_highlight_type & curr_highlight_type) != 0;
					const bool same_as_next = (curr_highlight_type & next_highlight_type) != 0;

					const bool is_edge_lt = !same_as_prev;
					const bool is_edge_rt = !same_as_next;

					PartialRectFlags sides = PartialRectFlags::PIPE_H;
					ImDrawFlags flags = ImDrawFlags_RoundCornersNone;

					if (is_edge_lt) {
						sides |= PartialRectFlags::LT;
						flags |= ImDrawFlags_RoundCornersLeft;
					}
					if (is_edge_rt) {
						sides |= PartialRectFlags::RT;
						flags |= ImDrawFlags_RoundCornersRight;
					}

					const auto gap_cross = settings.column_group_size
						&& (n % settings.column_group_size) == 0;

					const auto offset_x = (gap_cross * -1.0f) + sizes.col_group_spacing
						* gap_cross * ((prev_highlight_type & curr_highlight_type) != 0);

					const auto draw_pos = GetCursorScreenPos() - ImVec2(offset_x, 0.0f);

					const float bg_width = ((curr_highlight_type & next_highlight_type) != 0
						? (n != 0 && settings.column_group_size
							&& ((n + 1) % settings.column_group_size) == 0
						) + sizes.hex_cell_width : sizes.glyph_width * 2
					) + offset_x;

					draw_list->AddRectFilled(draw_pos, ImVec2(
						draw_pos.x + bg_width, draw_pos.y + line_height
					), curr_highlight_type.color, rounding, flags);

					DrawPartialRect(draw_pos, ImVec2(
						draw_pos.x + bg_width, draw_pos.y + line_height
					), GetColorU32(ImGuiCol_TabSelectedOverline),
						rounding, flags, sides);
				}

				// Carry the highlight objects forward for next byte so we can
				// keep track how exactly to apply highlighting
				prev_highlight_type = curr_highlight_type;
				curr_highlight_type = next_highlight_type;

				const auto cell_byte = callbacks.read
					? callbacks.read(internals.memory_data, address)
					: internals.memory_data[address];

				// Display text input on current byte ...
				if (internals.data_editing_address == address) {
					if (internals.acquire_edit_focus) {
						SetKeyboardFocusHere();
						std::snprintf(
							internals.goto_input_buffer.data(),
							internals.goto_input_buffer.size(),
							format_data, sizes.address_digit_count,
							internals.base_display_address + address
						);
						std::snprintf(
							internals.cell_input_buffer.data(),
							internals.cell_input_buffer.size(),
							format_byte, cell_byte
						);
					}

					struct InputTextUserData {
						std::array<char, 3> buffer{}; // Input
						s32  cursor_pos = -1; // Output

						auto cursor_at_end() const noexcept { return cursor_pos >= 2; }
						void reset_cursor() noexcept { cursor_pos = -1; }

						// FIXME: We should have a way to retrieve the text edit cursor position
						//        more easily in the API, this is rather tedious. This is such an
						//        ugly mess we may be better off not using InputText() at all here.
						static int callback(ImGuiInputTextCallbackData* data) {
							auto& user_data = *static_cast<InputTextUserData*>(data->UserData);
							if (!data->HasSelection()) { user_data.cursor_pos = data->CursorPos; }

							if (data->SelectionStart == 0 && data->SelectionEnd == data->BufTextLen) {
								// When not editing a byte, always refresh its InputText content pulled from underlying memory data
								// (this is a bit tricky, since InputText technically "owns" the master copy of the buffer we edit it in there)
								data->DeleteChars(0, data->BufTextLen);
								data->InsertChars(0, user_data.buffer.data());
								data->SelectionStart = data->CursorPos = 0;
								data->SelectionEnd = 2;
							}
							return 0;
						}
					} input_text_user_data;

					std::snprintf(
						input_text_user_data.buffer.data(),
						input_text_user_data.buffer.size(),
						format_byte, cell_byte
					);

					const auto flags = ImGuiInputTextFlags_CharsHexadecimal
						| ImGuiInputTextFlags_EnterReturnsTrue
						| ImGuiInputTextFlags_AutoSelectAll
						| ImGuiInputTextFlags_NoHorizontalScroll
						| ImGuiInputTextFlags_CallbackAlways
						| ImGuiInputTextFlags_AlwaysOverwrite
						| (settings.toggle_const_view ? ImGuiInputTextFlags_ReadOnly : 0);

					bool data_needs_writing = false;
					input_text_user_data.reset_cursor();

					PushID(reinterpret_cast<void*>(address));
					SetNextItemWidth(sizes.glyph_width * 2);
					const bool data_submitted = InputText(
						settings.toggle_const_view ? "##data_const" : "##data",
						internals.cell_input_buffer.data(),
						internals.cell_input_buffer.size(),
						flags, InputTextUserData::callback,
						&input_text_user_data
					);
					PopID();

					if (!data_submitted && !internals.acquire_edit_focus && !IsItemActive()) {
						internals.data_editing_address = data_editing_addr_next = c_max_addr;
						internals.acquire_edit_focus   = false;
						internals.cell_input_buffer[0] = '\0';
						advance_editing_cursor = data_needs_writing = false;
						continue;
					}

					internals.acquire_edit_focus = false;
					if (input_text_user_data.cursor_at_end()) {
						data_needs_writing = advance_editing_cursor = true;
					}
					if (data_editing_addr_next != c_max_addr) {
						data_needs_writing = advance_editing_cursor = false;
					}

					if (!settings.toggle_const_view && data_needs_writing) {
						const auto* begin = internals.cell_input_buffer.data();
						const auto* end   = begin + std::strlen(begin);

						u8 data_input_value = 0;
						const auto result = std::from_chars(begin, end, data_input_value, 16);

						if (result.ec == std::errc() && result.ptr == end) {
							if (callbacks.write) {
								callbacks.write(internals.memory_data, address, data_input_value);
							} else {
								internals.memory_data[address] = data_input_value;
							}
						}
					}
				}
				// ... or else display byte as text only
				else {
					// The trailing space is not visible but ensures there's
					// no gap that the mouse cannot click on.
					if (settings.toggle_hexii_view) {
						switch (cell_byte) {
							case 0x00:
								TextUnformatted("   ");
								break;
							case 0xFF:
								TextUnformatted("## ", cell_text_color_off);
								break;
							default:
								Text((cell_byte >= 32 && cell_byte < 127)
									? ".%c " : format_byte_space, cell_byte);
								break;
						}
					} else {
						if (cell_byte == 0x00 && settings.toggle_grey_zeroes) {
							TextUnformatted("00 ", cell_text_color_off);
						} else {
							Text(format_byte_space, cell_byte);
						}
					}

					if (IsItemHovered() && IsMouseClicked(ImGuiMouseButton_Left)) {
						internals.acquire_edit_focus = true;
						data_editing_addr_next = address;
					}
				}
			}

			// Segment: display each byte as ASCII on the right side if enabled
			if (settings.toggle_ascii_view) {
				SameLine(sizes.ascii_offset_min);
				auto pos = GetCursorScreenPos();
				address = std::size_t(line_i) * settings.column_count;

				const auto mouse_off_x = GetIO().MousePos.x - pos.x;
				const auto mouse_addr = (mouse_off_x >= 0.0f && mouse_off_x < sizes.ascii_offset_max - sizes.ascii_offset_min)
					? address + std::size_t(mouse_off_x / sizes.glyph_width) : c_max_addr;

				PushID(line_i);
				if (InvisibleButton("ascii", ImVec2(
					sizes.ascii_offset_max - sizes.ascii_offset_min, line_height)
				)) {
					internals.data_editing_address = internals.data_preview_address = mouse_addr;
					internals.acquire_edit_focus = true;
				}
				PopID();

				for (auto n = 0u; n < settings.column_count && address < internals.memory_size; ++n, ++address) {
					if (address == internals.data_editing_address) {
						draw_list->AddRectFilled(pos, ImVec2(
							pos.x + sizes.glyph_width, pos.y + line_height
						), settings.cell_emphasis_color, rounding);
					} else if (callbacks.bg_color) {
						draw_list->AddRectFilled(pos, ImVec2(
							pos.x + sizes.glyph_width, pos.y + line_height
						), callbacks.bg_color(internals.memory_data, address), rounding);
					}

					auto v = callbacks.read
						? callbacks.read(internals.memory_data, address)
						: internals.memory_data[address];

					char display_c = (v < 32 || v >= 128) ? '.' : v;
					draw_list->AddText(pos, (display_c == v)
						? cell_text_color : cell_text_color_off,
						&display_c, &display_c + 1
					);
					pos.x += sizes.glyph_width;
				}
			}
		}
	}
	PopStyleVar(2);
	const float child_width = GetWindowSize().x;
	EndChild();

	// Notify the main window of our ideal child content size (FIXME: we are missing an API to get the contents size from the child)
	const auto backup_pos = GetCursorScreenPos();
	SetCursorPosX(sizes.ascii_offset_max + GetStyle().ScrollbarSize
		+ GetStyle().WindowPadding.x * 2 + sizes.glyph_width);
	Dummy(ImVec2());
	SetCursorScreenPos(backup_pos);

	if (advance_editing_cursor && internals.data_editing_address + 1 < internals.memory_size) {
		assert(internals.data_editing_address != c_max_addr);
		internals.data_preview_address = internals.data_editing_address += 1;
		internals.acquire_edit_focus = true;
	} else if (data_editing_addr_next != c_max_addr) {
		internals.data_preview_address = internals.data_editing_address = data_editing_addr_next;
		internals.acquire_edit_focus = true;
	}

	if (settings.show_options_menu) {
		Separator();
		render_options_segment();
	}

	if (settings.show_data_preview) {
		Separator();
		render_preview_segment();
	}

	if (internals.goto_address < internals.memory_size) {
		BeginChild("##scrolling");
		SetScrollY(float(internals.goto_address / settings.column_count)
			* line_height - region_size.y * 0.5f);
		EndChild();

		internals.data_editing_address = internals.data_preview_address = internals.goto_address;
		internals.goto_address = c_max_addr;
		internals.acquire_edit_focus = true;
	}
	else if (internals.follow_address < internals.memory_size) {
		BeginChild("##scrolling");

		const auto cur_scroll_y = GetScrollY();
		const auto row_pos_top = line_height * float(
			internals.follow_address / settings.column_count);
		const auto row_pos_mid = row_pos_top + line_height * 0.5f;

		if (row_pos_mid < (cur_scroll_y + line_height)
			|| row_pos_mid > (region_size.y + cur_scroll_y - line_height)
		) {
			SetScrollY(row_pos_top - region_size.y * 0.5f);
		}
		EndChild();

		internals.follow_address = c_max_addr;
	}

	const auto contents_pos_end = ImVec2(
		contents_pos_start.x + child_width,
		GetCursorScreenPos().y
	);

	if (settings.show_options_menu) {
		if (IsMouseHoveringRect(contents_pos_start, contents_pos_end)) {
			if (IsWindowHovered(ImGuiHoveredFlags_ChildWindows)
				&& IsMouseReleased(ImGuiMouseButton_Right)
			) {
				OpenPopup("OptionsPopup");
			}
		}
	}

	if (BeginPopup("OptionsPopup")) {
		SetNextItemWidth(sizes.glyph_width * 16 + GetStyle().FramePadding.x * 2.0f);
		auto column_count = s32(settings.column_count);
		if (SliderInt("##cols", &column_count,
			settings.column_count.min, settings.column_count.max,
			"%d columns", ImGuiSliderFlags_AlwaysClamp
		)) {
			settings.column_count = u32(column_count);
		}
		Checkbox("Show Data Preview", &settings.show_data_preview);
		Checkbox("Show HexII", &settings.toggle_hexii_view);
		Checkbox("Show Ascii", &settings.toggle_ascii_view);
		Checkbox("Grey-out zeroes", &settings.toggle_grey_zeroes);
		Checkbox("Uppercase Hex", &settings.toggle_capital_hex);

		EndPopup();
	}
}

void MemoryEditor::render_options_segment() {
	using namespace ImGui;

	if (Button("Options")) {
		OpenPopup("OptionsPopup");
	}

	std::array<char, 64> formatted_range{};
	std::snprintf(formatted_range.data(), formatted_range.size(),
		settings.toggle_capital_hex ? "Range: %0*zX..%0*zX" : "Range: %0*zx..%0*zx",
		sizes.address_digit_count, internals.base_display_address,
		sizes.address_digit_count, internals.base_display_address + internals.memory_size - 1
	);

	SameLine(0, sizes.glyph_width);
	TextUnformatted(formatted_range.data());
	SameLine(0, sizes.glyph_width);
	SetNextItemWidth(GetStyle().FramePadding.x * 2.0f
		+ sizes.address_digit_count * sizes.glyph_width);

	if (InputText("##goto_address",
		internals.goto_input_buffer.data(), sizes.address_digit_count + 1,
		ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue
	)) {
		const char* begin = internals.goto_input_buffer.data();
		const char* end   = begin + std::strlen(begin);

		std::size_t goto_address = 0;

		const auto result = std::from_chars(begin, end, goto_address, 16);

		if (result.ec == std::errc() && result.ptr == end) {
			internals.goto_address = goto_address - internals.base_display_address;
			internals.highlight_min = internals.highlight_max = c_max_addr;
		}
	}
}

void MemoryEditor::render_preview_segment() {
	using namespace ImGui;

	AlignTextToFramePadding();
	TextUnformatted("Preview as:");

	SameLine(0, sizes.glyph_width);
	SetNextItemWidth(sizes.glyph_width * 9.0f
		+ GetStyle().FramePadding.x * 2.0f
		+ GetStyle().ItemInnerSpacing.x
	);

	if (BeginCombo("##combo_type", get_data_type_name(internals.preview_data_type), ImGuiComboFlags_HeightLarge)) {
		for (auto type = 0; type < DataType::COUNT; ++type) {
			const auto data_type = static_cast<DataType>(type);
			if (Selectable(get_data_type_name(data_type), internals.preview_data_type == data_type)) {
				internals.preview_data_type = data_type;
			}
		}
		EndCombo();
	}

	SameLine(0, sizes.glyph_width);
	SetNextItemWidth((sizes.glyph_width * 6.0f)
		+ GetStyle().FramePadding.x * 2.0f
		+ GetStyle().ItemInnerSpacing.x
	);

	static constexpr const char* endianness_labels[] = { "LE", "BE" };
	//Combo("##combo_endianness", &internals.preview_endianness, "LE\0BE\0\0");
	if (BeginCombo("##combo_endianness", endianness_labels[internals.preview_endianness], ImGuiComboFlags_HeightSmall)) {
		if (Selectable(endianness_labels[Endian::LE], internals.preview_endianness == Endian::LE)) {
			internals.preview_endianness = Endian::LE;
		}
		if (Selectable(endianness_labels[Endian::BE], internals.preview_endianness == Endian::BE)) {
			internals.preview_endianness = Endian::BE;
		}
		EndCombo();
	}

	std::array<char, 128> text_buffer{};
	const auto text_buffer_span = std::span(text_buffer);

	std::array<u8, 8> byte_scratchpad{}; // max size
	const auto byte_scratchpad_span = std::span(byte_scratchpad.data(),
		get_data_type_size(internals.preview_data_type));

	const auto source_memory = std::span(internals.memory_data, internals.memory_size);

	const bool have_valid_address = internals.data_preview_address != c_max_addr;
	if (have_valid_address) {
		read_bytes(source_memory, byte_scratchpad_span,
			internals.data_preview_address);
	}

	const auto render_formatted_text = [&](const char* label, DataFormat fmt) {
		if (have_valid_address) {
			format_value(internals.preview_data_type,
				text_buffer_span, byte_scratchpad_span, fmt);
		}

		TextUnformatted(label);
		SameLine(sizes.glyph_width * 5.0f);
		TextUnformatted(have_valid_address
			? text_buffer.data() : "N/A");
	};

	render_formatted_text("Dec:", DataFormat::DEC);
	render_formatted_text("Hex:", DataFormat::HEX);
	render_formatted_text("Bin:", DataFormat::BIN);
	text_buffer.back() = '\0'; // safety measure
}
