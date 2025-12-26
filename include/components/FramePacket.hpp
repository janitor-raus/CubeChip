/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "EzMaths.hpp"
#include "ColorOps.hpp"
#include "Aligned.hpp"
#include "Concepts.hpp"

/*==================================================================*/

template <typename SrcT, typename Ufn>
using unary_transform_result_t = std::conditional_t<
	std::is_invocable_v<Ufn&, SrcT&>,
	std::invoke_result_t<Ufn&, SrcT&>,
	std::invoke_result_t<Ufn&, const SrcT&>
>;

template <typename SrcT, typename Bfn>
using binary_transform_result_t = std::conditional_t<
	std::is_invocable_v<Bfn&, SrcT&, SrcT&>,
	std::invoke_result_t<Bfn&, SrcT&, SrcT&>,
	std::invoke_result_t<Bfn&, const SrcT&, const SrcT&>
>;

/*==================================================================*/

struct FramePacket {
	using value_type = std::byte;
	using pointer = value_type*;

	class Metadata {
		using self = Metadata;

	private:
		ez::Frame base_frame;

		constexpr auto make_base_frame(ez::s32 w, ez::s32 h) const noexcept {
			return ez::Frame(std::clamp(w, 1, 8192), std::clamp(h, 1, 8192));
		}

	public:
		constexpr auto get_base_frame() const noexcept { return base_frame; }

	private:
		ez::Rect viewport_rect;

	public:
		constexpr auto  get_viewport() const noexcept { return viewport_rect; }
		constexpr self& set_viewport(ez::s32 w, ez::s32 h, ez::s32 x = 0, ez::s32 y = 0) noexcept {
			viewport_rect.x = std::clamp(x, 0, base_frame.w - 1);
			viewport_rect.y = std::clamp(y, 0, base_frame.h - 1);
			viewport_rect.w = std::clamp(w, 1, base_frame.w - viewport_rect.x);
			viewport_rect.h = std::clamp(h, 1, base_frame.h - viewport_rect.y);
			return *this;
		}

		constexpr auto get_flipped_viewport_if(bool cond) const noexcept {
			return cond ? ez::Rect(
				viewport_rect.y, viewport_rect.x,
				viewport_rect.h, viewport_rect.w
			) : viewport_rect;
		}

		// RGB color applied as background when texture padding is used.
		// The alpha channel is used to fade the texture to black.
		RGBA texture_tint = RGBA::White;
		constexpr self& set_texture_tint(ez::u32 color) noexcept {
			texture_tint = color;
			return *this;
		}

	private:
		ez::u8 upscale_mult = 1;

	public:
		constexpr auto  get_scaling() const noexcept { return upscale_mult; }
		// Scales minimum texture display size from 1x to 16x
		constexpr self& set_scaling(ez::s32 scale) noexcept {
			upscale_mult = ez::u8(std::clamp(scale, 1, 16));
			return *this;
		}

	private:
		ez::u8 texture_pad = 0;

	public:
		constexpr auto  get_padding() const noexcept {
			return texture_pad;
		}
		// Adds virtual padding pixels around the texture display (max: 15)
		constexpr self& set_padding(ez::s32 width) noexcept {
			texture_pad = ez::u8(std::clamp(width, 0, 32));
			return *this;
		}

	public:
		//ez::u8 debug_mode{}; // debug mode flag (signal for frontend)

		bool interlaced{}; // signal is interlaced, framerate is halved
		bool enabled{};    // enables the display (for systems that need it off)

		static constexpr RGBA default_border_color = RGBA(0x303030FF);
		RGBA border_color = default_border_color;
		ez::f32 border_width = 4.0f;

		constexpr self& set_border_color_if(bool cond, RGBA color) noexcept {
			border_color = cond ? color : default_border_color;
			return *this;
		}

		constexpr self& set_border_color_if(bool cond, RGBA color1, RGBA color2) noexcept {
			border_color = cond ? color1 : color2;
			return *this;
		}

		ez::f32 refresh_rate = 60.0f; // display refresh rate in Hz
		ez::f32 pixel_ratio  =  1.0f; // width/height ratio of a single pixel

		ez::u64 frame_count{}; // number of frames rendered

	public:
		Metadata(ez::s32 W, ez::s32 H) noexcept
			: base_frame(make_base_frame(W, H))
			, viewport_rect(0, 0, W, H)
		{}

		Metadata(Metadata&&) = default;
		Metadata& operator=(Metadata&&) = default;

		Metadata(const Metadata&) = default;
		Metadata& operator=(const Metadata& other) noexcept {
			if (this != &other) {
				set_viewport(
					other.viewport_rect.w, other.viewport_rect.h,
					other.viewport_rect.x, other.viewport_rect.y
				);

				texture_tint = other.texture_tint;
				upscale_mult = other.upscale_mult;
				texture_pad  = other.texture_pad;
				interlaced   = other.interlaced;
				enabled      = other.enabled;
				border_color = other.border_color;
				border_width = other.border_width;
				refresh_rate = other.refresh_rate;
				pixel_ratio  = other.pixel_ratio;
				frame_count  = other.frame_count;
			}
			return *this;
		}

		// Calculates Screen Aspect Ratio (as: viewport.W / viewport.H)
		constexpr auto calc_sar() const noexcept {
			return ez::f32(viewport_rect.w) / ez::f32(viewport_rect.h);
		}
		// Calculates Display Aspect Ratio (as: SAR * Pixel Aspect Ratio)
		constexpr auto calc_dar() const noexcept {
			return calc_sar() * pixel_ratio;
		}
		constexpr auto calc_effective_framerate() const noexcept {
			return interlaced ? (refresh_rate * 0.5f) : refresh_rate;
		}
	};

private:
	AlignedUniqueArray
		<value_type> m_buffer;

public:
	Metadata metadata;

	pointer data()       noexcept { return m_buffer.get(); }
	pointer data() const noexcept { return m_buffer.get(); }

	auto size() const noexcept { return metadata.get_base_frame().area(); }

private:
	auto clamp_count(std::size_t count) const noexcept {
		return count ? std::min<std::size_t>(count, size()) : size();
	}

public:
	FramePacket(std::size_t SIZE, std::size_t W, std::size_t H) noexcept
		: m_buffer(::allocate_n<value_type>(W * H * SIZE).as_value().release())
		, metadata(static_cast<int>(W), static_cast<int>(H))
	{}

public:
	template <typename SourceT>
	void copy_from(const SourceT* src, std::size_t count = 0) noexcept {
		static_assert(alignof(SourceT) <= HDIS);
		static_assert(std::is_trivially_copyable_v<SourceT>,
			"SourceT must be trivially copyable.");

		auto dst_rc = reinterpret_cast<SourceT*>(data());
		std::copy_n(EXEC_POLICY(unseq)
			src, clamp_count(count), dst_rc);
	}

	template <IsContiguousContainer T>
	void copy_from(const T& src) noexcept {
		copy_from(src.data(), src.size());
	}

	template <typename SourceT, typename Ufn>
	void copy_from(const SourceT* src, std::size_t count, Ufn&& fn) noexcept {
		using OutputT = unary_transform_result_t<SourceT, Ufn>;
		static_assert(alignof(OutputT) <= HDIS);
		static_assert(std::is_trivially_copyable_v<SourceT>,
			"SourceT must be trivially copyable.");
		static_assert(std::is_trivially_copyable_v<OutputT>,
			"Buffer data type must be trivially copyable.");

		auto dst_rc = reinterpret_cast<OutputT*>(data());
		std::transform(EXEC_POLICY(unseq)
			src, src + clamp_count(count),
			dst_rc, std::forward<Ufn>(fn));
	}

	template <IsContiguousContainer T, typename Ufn>
	void copy_from(const T& src, Ufn&& fn) noexcept {
		copy_from(src.data(), src.size(), std::forward<Ufn>(fn));
	}

	template <typename SourceT, typename Bfn>
	void copy_from(const SourceT* src1, std::size_t count1, const SourceT* src2, std::size_t count2, Bfn&& fn) noexcept {
		using OutputT = binary_transform_result_t<SourceT, Bfn>;
		static_assert(alignof(OutputT) <= HDIS);
		static_assert(std::is_trivially_copyable_v<SourceT>,
			"SourceT must be trivially copyable.");
		static_assert(std::is_trivially_copyable_v<OutputT>,
			"Buffer data type must be trivially copyable.");

		auto dst_rc = reinterpret_cast<OutputT*>(data());
		std::transform(EXEC_POLICY(unseq)
			src1, src1 + clamp_count(std::min(count1, count2)),
			src2, dst_rc, std::forward<Bfn>(fn));
	}

	template <IsContiguousContainer T, typename Bfn>
	void copy_from(const T& src1, const T& src2, Bfn&& fn) noexcept {
		copy_from(src1.data(), src1.size(), src2.data(), src2.size(), std::forward<Bfn>(fn));
	}

	/*==================================================================*/

	template <typename OutputT>
	void copy_into(OutputT* dst, std::size_t count = 0) const noexcept {
		static_assert(std::is_trivially_copyable_v<OutputT>,
			"OutputT must be trivially copyable.");

		auto src_rc = reinterpret_cast<const OutputT*>(data());
		std::copy_n(EXEC_POLICY(unseq)
			src_rc, clamp_count(count), dst);
	}

	template <IsContiguousContainer T>
	void copy_into(T& dst) const noexcept {
		copy_into(dst.data(), dst.size());
	}

	template <typename OutputT, typename Ufn>
	void copy_into(OutputT* dst, std::size_t count, Ufn&& fn) const noexcept {
		static_assert(alignof(OutputT) <= HDIS);
		static_assert(std::is_trivially_copyable_v<OutputT>,
			"OutputT must be trivially copyable.");

		using RetT = unary_transform_result_t<OutputT, Ufn>;
		static_assert(std::is_convertible_v<RetT, OutputT>,
			"Ufn return type must be convertible to OutputT.");

		auto src_rc = reinterpret_cast<const OutputT*>(data());
		std::transform(EXEC_POLICY(unseq)
			src_rc, src_rc + clamp_count(count),
			dst, std::forward<Ufn>(fn));
	}

	template <IsContiguousContainer T, typename Ufn>
	void copy_into(T& dst, Ufn&& fn) const noexcept {
		copy_into(dst.data(), dst.size(), std::forward<Ufn>(fn));
	}
};

static_assert(std::is_move_constructible_v<FramePacket>, "FramePacket must be move-constructible.");
static_assert(std::is_move_assignable_v   <FramePacket>, "FramePacket must be move-assignable.");
