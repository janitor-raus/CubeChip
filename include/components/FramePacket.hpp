/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "EzMaths.hpp"
#include "ColorOps.hpp"
#include "Parameter.hpp"
#include "Aligned.hpp"
#include "Concepts.hpp"

/*==================================================================*/

template <typename F, typename T>
using UnaryReturnT = std::invoke_result_t<F&, T&>;

/**
 * @brief Concept for a unary transform callable, which must be nothrow invocable
 * with one parameter of type T and return a nothrow copyable type.
 */
template <typename F, typename T>
concept UnaryTransformFn = std::invocable<F&, T&>
	&& std::is_nothrow_invocable_v<F&, T&>
	&& IsNothrowCopyable<UnaryReturnT<F, T>>;

/*==================================================================*/

template <typename F, typename T>
using BinaryReturnT = std::invoke_result_t<F&, T&, T&>;

/**
 * @brief Concept for a binary transform callable, which must be nothrow invocable
 * with two parameters of type T and return a nothrow copyable type.
 */
template <typename F, typename T>
concept BinaryTransformFn = std::invocable<F&, T&, T&>
	&& std::is_nothrow_invocable_v<F&, T&, T&>
	&& IsNothrowCopyable<BinaryReturnT<F, T>>;

/*==================================================================*/

struct FramePacket {
	using value_type = std::byte;
	using pointer = value_type*;
	using const_pointer = const value_type*;

	class Metadata {
		friend struct FramePacket;

	private:
		ez::Frame base_frame;

		constexpr auto make_base_frame(s32 w, s32 h) const noexcept {
			return ez::Frame(std::clamp(w, 1, 8192), std::clamp(h, 1, 8192));
		}

	public:
		constexpr auto get_base_frame() const noexcept { return base_frame; }

	private:
		ez::Rect viewport_rect;

	public:
		constexpr auto get_viewport() const noexcept { return viewport_rect; }
		constexpr void set_viewport(s32 w, s32 h, s32 x = 0, s32 y = 0) noexcept {
			viewport_rect.x = std::clamp(x, 0, base_frame.w - 1);
			viewport_rect.y = std::clamp(y, 0, base_frame.h - 1);
			viewport_rect.w = std::clamp(w, 1, base_frame.w - viewport_rect.x);
			viewport_rect.h = std::clamp(h, 1, base_frame.h - viewport_rect.y);
		}

	public:
		// The color components (RGB_) are used as the background color.
		// The alpha component (___A) is used to fade out the texture instead.
		RGBA texture_tint = RGBA::White;

		// The color of the border drawn around the texture (if border_width > 0).
		// The alpha component (___A) is unused.
		RGBA border_color = default_border_color;

		static constexpr RGBA default_border_color = RGBA(0x303030FF);

		constexpr void set_border_color_if(bool cond, RGBA color) noexcept {
			border_color = cond ? color : default_border_color;
		}

		// Forces a mininum zoom level for the texture (1-16x)
		BoundedParam<1, 1, 16> minimum_zoom;

		// Adds inner margin between the texture and border (max: 32px)
		BoundedParam<0, 0, 32> inner_margin;

		u8 debug_flags{}; // debug mode flag (signal for frontend use)

		bool debug_mode{}; // enable debug mode,
		bool interlaced{}; // signal is interlaced, framerate is halved
		bool enabled{};    // enables the display (for systems that need it off)

		BoundedParam<4, 0, 32> border_width;

		// Sets the texture's pixel aspect ratio (0.1..4.0)
		BoundedParam<1.0f, 0.1f, 4.0f> pixel_ratio;

		// Describes the display's refresh rate in Hz, but has no effect on the actual framerate of the system.
		BoundedParam<60.0f, 1.0f, 1000.0f> refresh_rate;

	public:
		Metadata(s32 W, s32 H) noexcept
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
				minimum_zoom = other.minimum_zoom;
				inner_margin = other.inner_margin;
				interlaced   = other.interlaced;
				enabled      = other.enabled;
				border_color = other.border_color;
				border_width = other.border_width;
				refresh_rate = other.refresh_rate;
				pixel_ratio  = other.pixel_ratio;
			}
			return *this;
		}

		// Calculates Screen Aspect Ratio (as: viewport.W / viewport.H)
		constexpr auto calc_sar() const noexcept {
			return f32(viewport_rect.w) / f32(viewport_rect.h);
		}
		// Calculates Display Aspect Ratio (as: SAR * Pixel Aspect Ratio)
		constexpr auto calc_dar() const noexcept {
			return calc_sar() * pixel_ratio;
		}
		constexpr auto calc_effective_framerate() const noexcept {
			return refresh_rate * (interlaced ? 0.5f : 1.0f);
		}
	};

private:
	static constexpr auto MAX_ALIGN = ::HDIS;

	AlignedUniqueArray
		<value_type, MAX_ALIGN> m_buffer;

public:
	Metadata metadata;

	/***/ pointer data()       noexcept { return m_buffer.get(); }
	const_pointer data() const noexcept { return m_buffer.get(); }

	auto size() const noexcept { return metadata.get_base_frame().area(); }

private:
	auto clamp_count(std::size_t count) const noexcept {
		return count ? std::min<std::size_t>(count, size()) : size();
	}

public:
	FramePacket(std::size_t W, std::size_t H, std::size_t bpp = sizeof(RGBA)) noexcept
		: m_buffer(::allocate_n<value_type, MAX_ALIGN>(W * H * bpp).as_value().release())
		, metadata(static_cast<int>(W), static_cast<int>(H))
	{}

public:
	template <IsNothrowCopyable Src>
	void copy_from(const Src* src, std::size_t count = 0) noexcept {
		static_assert(alignof(Src) <= MAX_ALIGN,
			"Source type alignment exceeds buffer alignment!");

		auto dst_rc = reinterpret_cast<Src*>(data());
		std::copy_n(EXEC_POLICY(unseq)
			src, clamp_count(count), dst_rc);
	}

	template <IsContiguousContainer Src> requires (IsNothrowCopyable<ValueType<Src>>)
	void copy_from(const Src& src) noexcept {
		copy_from(src.data(), src.size());
	}

	template <IsNothrowCopyable Src, UnaryTransformFn<Src> Ufn>
	void copy_from(const Src* src, std::size_t count, Ufn&& fn) noexcept {
		using Out = UnaryReturnT<Ufn, Src>;
		static_assert(alignof(Out) <= MAX_ALIGN,
			"Output type alignment exceeds buffer alignment!");

		auto dst_rc = reinterpret_cast<Out*>(data());
		std::transform(EXEC_POLICY(unseq)
			src, src + clamp_count(count),
			dst_rc, std::forward<Ufn>(fn));
	}

	template <IsContiguousContainer Src, UnaryTransformFn<ValueType<Src>> Ufn> requires (IsNothrowCopyable<ValueType<Src>>)
	void copy_from(const Src& src, Ufn&& fn) noexcept {
		copy_from(src.data(), src.size(), std::forward<Ufn>(fn));
	}

	template <IsNothrowCopyable Src, BinaryTransformFn<Src> Bfn>
	void copy_from(const Src* src1, std::size_t count1, const Src* src2, std::size_t count2, Bfn&& binary_function) noexcept {
		using Out = BinaryReturnT<Bfn, Src>;
		static_assert(alignof(Out) <= MAX_ALIGN,
			"Output type alignment exceeds buffer alignment!");

		auto dst_rc = reinterpret_cast<Out*>(data());
		std::transform(EXEC_POLICY(unseq)
			src1, src1 + clamp_count(std::min(count1, count2)),
			src2, dst_rc, std::forward<Bfn>(binary_function));
	}

	template <IsContiguousContainer Src, BinaryTransformFn<ValueType<Src>> Bfn> requires (IsNothrowCopyable<ValueType<Src>>)
	void copy_from(const Src& src1, const Src& src2, Bfn&& binary_function) noexcept {
		copy_from(src1.data(), src1.size(), src2.data(), src2.size(), std::forward<Bfn>(binary_function));
	}

	/*==================================================================*/

	template <IsNothrowCopyable Dst>
	void copy_into(Dst* dst, std::size_t count = 0) const noexcept {

		auto src_rc = reinterpret_cast<const Dst*>(data());
		std::copy_n(EXEC_POLICY(unseq)
			src_rc, clamp_count(count), dst);
	}

	template <IsContiguousContainer Dst> requires (IsNothrowCopyable<ValueType<Dst>>)
	void copy_into(Dst& dst) const noexcept {
		copy_into(dst.data(), dst.size());
	}

	template <IsNothrowCopyable Dst, UnaryTransformFn<Dst> Ufn> requires (IsNothrowConvertible<UnaryReturnT<Ufn, Dst>, Dst>)
	void copy_into(Dst* dst, std::size_t count, Ufn&& fn) const noexcept {
		using Out = UnaryReturnT<Ufn, Dst>;
		static_assert(alignof(Out) <= MAX_ALIGN,
			"Output type alignment exceeds buffer alignment!");

		auto src_rc = reinterpret_cast<const Dst*>(data());
		std::transform(EXEC_POLICY(unseq)
			src_rc, src_rc + clamp_count(count),
			dst, std::forward<Ufn>(fn));
	}

	template <IsContiguousContainer Dst, UnaryTransformFn<ValueType<Dst>> Ufn> requires (IsNothrowCopyable<ValueType<Dst>>)
	void copy_into(Dst& dst, Ufn&& fn) const noexcept {
		copy_into(dst.data(), dst.size(), std::forward<Ufn>(fn));
	}
};

static_assert(IsNothrowMovable<FramePacket>,
	"FramePacket must be nothrow move-constructible/assignable.");
