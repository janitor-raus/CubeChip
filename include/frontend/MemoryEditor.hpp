/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

// Usage:
//   // If you already have a window, use DrawContents() instead:
//   static MemoryEditor mem_edit_2;
//   ImGui::Begin("MyWindow")
//   mem_edit_2.DrawContents(this, sizeof(*this), (size_t)this);
//   ImGui::End();

// TODO:
// - This is generally old/crappy code, it should work but isn't very good.. to be rewritten some day.
// - PageUp/PageDown are not supported because we use _NoNav. This is a good test scenario for working out idioms of how to mix natural nav and our own...
// - Arrows are being sent to the InputText() about to disappear which for LeftArrow makes the text cursor appear at position 1 for one frame.
// - Using InputText() is awkward and maybe overkill here, consider implementing something custom.

#include <type_traits>
#include <array>
#include <functional>
#include <span>
#include <cstdio>

#include "EzMaths.hpp"
#include "ColorOps.hpp"
#include "Parameter.hpp"

/*==================================================================*/

struct MemoryEditor {
	enum DataFormat {
		BIN, // Binary
		DEC, // Decimal
		HEX, // Hexadecimal
		BAR, // Code 128(B)
	};

	enum DataType : u8 {
		S8, U8,
		S16, U16,
		S32, U32,
		S64, U64,
		F32, F64,
		COUNT
	};

	enum Endian {
		LE, BE,
	};

	struct Settings {
		bool window_visible_out = false; // Can be used externally to toggle window visibility, has no effect on rendering the widget itself.
		bool show_options_menu  = true;  // Enable options button/context menu. When disabled, options will be locked unless you provide your own UI for them.
		bool show_data_preview  = false; // Enable a footer area previewing the decimal/binary/hex/float representation of the currently selected bytes.

		bool toggle_const_view  = false; // Prevent editing of the memory contents, ensuring constness.
		bool toggle_hexii_view  = false; // Display values in HexII representation instead of regular hexadecimal: hide null/zero bytes, ascii values as ".X".
		bool toggle_ascii_view  = true;  // Display ASCII representation on the right side of the Memory Viewer.
		bool toggle_grey_zeroes = true;  // Grey-out null/zero bytes using the TextDisabled color.
		bool toggle_capital_hex = false; // Present hexadecimal values as "FF" instead of "ff".

		BoundedParam<u32(8), 4, 32> column_count;      // Number of columns to display.
		BoundedParam<u32(8), 0, 16> column_group_size; // Insert spacing between N columns to separate groups. Use 0 to disable.
		u32  address_digit_count = 0;                  // Maximum number of address digits to display (minimum enforced automatically to cover memory range).
		f32  footer_area_height  = 0.0f;               // Space in px to reserve at the bottom of the widget to add custom widgets.
		RGBA cell_emphasis_color = RGBA();             // Background color of highlighted bytes.
	} settings;

	void goto_address(std::size_t min, std::size_t len = 0) noexcept {
		internals.goto_address = internals.highlight_min = min;
		internals.highlight_max = min + (len ? len : 1);
	}
	void follow_address(std::size_t min, std::size_t len = 1, bool scroll = true) noexcept {
		internals.highlight_min = min;
		internals.highlight_max = min + (len ? len : 1);
		internals.follow_address = scroll ? min : c_max_addr;
	}

	struct Callbacks {
		std::function<  u8(const u8* mem, std::size_t pos /********/)> read = nullptr;  // Optional handler to read bytes.
		std::function<void(/***/ u8* mem, std::size_t pos,  u8 value)> write = nullptr; // Optional handler to write bytes.
		std::function<bool(const u8* mem, std::size_t pos /********/)> highlight = nullptr; // Optional handler to return Highlight property (to support non-contiguous highlighting).
		std::function<RGBA(const u8* mem, std::size_t pos)> bg_color  = nullptr; // Optional handler to return custom background color of individual bytes.
	} callbacks;

private:
	static constexpr auto c_max_addr = std::numeric_limits<std::size_t>::max();

	static const char* get_data_type_name(DataType data_type) {
		static constexpr const char* descs[] = {
			"Int8",  "Uint8",
			"Int16", "Uint16",
			"Int32", "Uint32",
			"Int64", "Uint64",
			"Float", "Double",
		};
		return descs[data_type];
	}

	static auto get_data_type_size(DataType data_type) {
		static constexpr std::size_t sizes[] = {
			sizeof(s8),  sizeof(u8),
			sizeof(s16), sizeof(u16),
			sizeof(s32), sizeof(u32),
			sizeof(s64), sizeof(u64),
			sizeof(f32), sizeof(f64),
		};
		return sizes[data_type];
	}

private:
	struct Internals {
		using CharBuffer = std::array<char, 32>;

		CharBuffer  cell_input_buffer{};
		CharBuffer  goto_input_buffer{};

		std::size_t data_preview_address = c_max_addr;
		std::size_t data_editing_address = c_max_addr;

		std::size_t goto_address   = c_max_addr;
		std::size_t highlight_min  = c_max_addr;
		std::size_t highlight_max  = c_max_addr;
		std::size_t follow_address = c_max_addr;

		u8*         memory_data = nullptr;
		std::size_t memory_size = 0;
		std::size_t base_display_address = 0;

		Endian      preview_endianness = Endian::LE;
		DataType    preview_data_type  = DataType::S32;
		bool        acquire_edit_focus = false;
	} internals;

public:
	void set_preview_endianness(Endian endianness) noexcept {
		internals.preview_endianness = endianness;
	}

private:
	enum HighlightType { None = 0, Follow = 1, Callback = 2, Preview = 4, BgColor = 8 };
	struct HighlightInfo {
		u32  type = HighlightType::None;
		RGBA color = RGBA::Transparent;

		HighlightInfo() noexcept = default;
		HighlightInfo(HighlightType type_, RGBA color_) noexcept
			: type(type_), color(color_) {}

		constexpr operator u32() const noexcept { return type; }
		constexpr bool operator==(const HighlightInfo& other) const noexcept {
			return this->type == other.type;
		}
	};

	auto get_highlight_type(std::size_t address) noexcept -> HighlightInfo;

	struct Sizes {
		u32 address_digit_count{}; // Number of digits required to represent maximum address.
		f32 glyph_width{};         // Glyph width (assume mono-space).
		f32 hex_cell_width{};      // Width of a hex edit cell ~2.5f * glyph_width.
		f32 col_group_spacing{};   // Spacing between each columns section (column_group_size).
		f32 hex_offset_min{};
		f32 hex_offset_max{};
		f32 ascii_offset_min{};
		f32 ascii_offset_max{};
	} sizes;

	void recalculate_all_sizes();

public:
	MemoryEditor() noexcept = default;

	/**
	 * @brief Sets the memory range to display/edit. Must be called at least once before drawing the widget, and must be called again if the memory range, its size or base display address changes.
	 * @tparam T Type of the memory data, used to determine constness and element size. The memory will be interpreted as an array of T.
	 * @param mem_data Pointer to the memory data to display/edit. The memory is expected to be contiguous data.
	 * @param mem_size Number of elements of type T in the memory range. The total byte size is mem_size * sizeof(T).
	 * @param base_display_address Base address to display for the start of the memory range. This is only for display purposes and does not affect how memory is accessed or indexed.
	 */
	template <typename T>
	void set_memory_range(
		T* memory_data, std::size_t memory_size,
		std::size_t base_display_address = 0
	) noexcept {
		settings.toggle_const_view = std::is_const_v<T>;
		internals.memory_data = reinterpret_cast<u8*>(const_cast<T*>(memory_data));
		internals.memory_size = memory_size * sizeof(T);
		internals.base_display_address = base_display_address;
		recalculate_all_sizes();
	}

private:
	void read_bytes(
		std::span<const u8> src,
		std::span</***/ u8> dst,
		std::size_t addr
	) const;

	void endian_aware_copy(void* dst, const void* src, size_t size) const;

	template <typename T>
	void format_binary(std::span<char> out, const T* in) const {
		const u8* r_in = reinterpret_cast<const u8*>(in);
		for (auto c = s32(sizeof(T) - 1), i = 0; c >= 0; --c, ++i) {
			for (auto b = 0; b < 8; ++b, ++i) {
				out[i] = r_in[c] & (1 << (7 - b)) ? '1' : '0';
			}
			out[i] = (c > 0) ? ' ' : '\0';
		}
	}

	template <typename T>
	void typed_formatter(std::span<char> output, const T& data, DataFormat fmt) {
		if (fmt == DataFormat::BIN) {
			format_binary(output, &data);
			return;
		}

		if constexpr (std::is_floating_point_v<T>) {
			if (fmt == DataFormat::DEC) {
				std::snprintf(output.data(), output.size(), "%g", data);
			} else {
				std::snprintf(output.data(), output.size(), "%a", data);
			}
		} else {
			if (fmt == DataFormat::DEC) {
				if constexpr (std::is_signed_v<T>) {
					std::snprintf(output.data(), output.size(), "%lld", s64(data));
				} else {
					std::snprintf(output.data(), output.size(), "%llu", u64(data));
				}
			} else {
				std::snprintf(output.data(), output.size(),
					settings.toggle_capital_hex ? "0x%0*llX" : "0x%0*llx",
					s32(sizeof(T) * 2), u64(std::make_unsigned_t<T>(data)));
			}
		}
	}

	template <typename T>
	T load_value(std::span<const u8> bytes) const {
		static_assert(std::is_trivially_copyable_v<T>);

		T value{};
		endian_aware_copy(&value, bytes.data(), sizeof(T));
		return value;
	}

	void format_value(
		DataType type, std::span<char> output,
		std::span<const u8> bytes, DataFormat fmt
	);

public:
	void render_memory_editor();

private:
	void render_options_segment();
	void render_preview_segment();
};
