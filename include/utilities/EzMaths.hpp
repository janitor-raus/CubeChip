/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>
#include <concepts>
#include <algorithm>

#include "Macros.hpp"

/*==================================================================*/

#if defined(__cpp_constexpr) && (__cpp_constexpr >= 202211L)
	#define CONSTEXPR_MATH constexpr
#else
	#define CONSTEXPR_MATH
#endif

/*==================================================================*/

namespace EzMaths {
	using f32 = float;
	using f64 = double;
	using u8  = std::uint8_t;
	using s8  = std::int8_t;
	using u16 = std::uint16_t;
	using s16 = std::int16_t;
	using u32 = std::uint32_t;
	using s32 = std::int32_t;
	using u64 = std::uint64_t;
	using s64 = std::int64_t;
}

/*==================================================================*/

namespace EzMaths {
	struct alignas(sizeof(s32) * 2) Point {
		s32 x{}, y{};

		constexpr Point(s32 x = 0, s32 y = 0) noexcept
			: x{ x }, y{ y }
		{}

		constexpr auto operator+(const Point& other) const noexcept
			{ return Point(x + other.x, y + other.y); }
	};

	struct alignas(sizeof(s32) * 2) Frame {
		s32 w{}, h{};

		constexpr Frame(s32 w = 0, s32 h = 0) noexcept
			: w{ w < 0 ? 0 : w }
			, h{ h < 0 ? 0 : h }
		{}

		constexpr auto area() const noexcept { return 1ull * w * h; }
		constexpr auto half() const noexcept { return Point(w / 2, h / 2); }

		constexpr bool operator==(const Frame& other) const noexcept
			{ return w == other.w && h == other.h; }
	};

	struct Rect : public Point, public Frame {
		constexpr Rect(s32 x = 0, s32 y = 0, s32 w = 0, s32 h = 0) noexcept
			: Point(x, y), Frame(w, h)
		{}
		constexpr Rect(Point point, Frame frame = {}) noexcept
			: Point(point), Frame(frame)
		{}
		constexpr Rect(Frame frame, Point point = {}) noexcept
			: Point(point), Frame(frame)
		{}

		constexpr auto as_point() const noexcept
			{ return Point(x, y); }

		constexpr auto as_frame() const noexcept
			{ return Frame(w, h); }

		constexpr auto center() const noexcept
			{ return half() + as_point(); }
	};

	// Lightweight, unprotected Weight class with 8-bit integer precision.
	// Expected constructor ranges: [0..255] for integers, [0..1] for floats.
	class Weight {
		u8 mWeight{};

	public:
		template <std::integral Int>
		constexpr Weight(Int value) noexcept : mWeight{ u8(value) } {}
		constexpr Weight(f64 value) noexcept : mWeight{ u8(value * 255.0) } {}

		// Cast weight to floating-point [0..1] value
		constexpr auto as_fp() const noexcept {
			return (1.0 / 255.0) * mWeight;
		}

		constexpr operator u8() const noexcept { return mWeight; }
	};
}

/*==================================================================*/

namespace EzMaths {
	inline constexpr auto intersect(const Rect& lhs, const Rect& rhs) noexcept {
		auto x1{ std::max(lhs.x, rhs.x) };
		auto y1{ std::max(lhs.y, rhs.y) };
		auto x2{ std::min(lhs.x + lhs.w, rhs.x + rhs.w) };
		auto y2{ std::min(lhs.y + lhs.h, rhs.y + rhs.h) };

		auto w{ std::max(0, x2 - x1) };
		auto h{ std::max(0, y2 - y1) };
		auto x{ w > 0 ? x1 : 0 };
		auto y{ h > 0 ? y1 : 0 };

		return Rect(x, y, w, h);
	}

	inline constexpr auto distance(const Point& lhs, const Point& rhs) noexcept {
		s64 dx{ lhs.x - rhs.x };
		s64 dy{ lhs.y - rhs.y };
		return u64(dx * dx) + u64(dy * dy);
	}
}

/*==================================================================*/

namespace EzMaths {
	template <typename T> requires (std::is_signed_v<T>)
	inline constexpr T abs(T x) noexcept
		{ return x < T(0) ? -x : x; }

	// simple constexpr-enabled fmod, internally allows s32-width division
	template <std::floating_point T>
	inline constexpr T fmod(T x, T y) noexcept
		{ return y ? x - y * int(x / y) : x; }

	template <std::floating_point T>
	inline constexpr T round(T x) noexcept {
		return x >= T(0)
			? T(static_cast<long long>(x + T(0.5)))
			: T(static_cast<long long>(x - T(0.5)));
	}

	// simple constexpr-enabled tanh approximation, mostly-par up to x of 3.0
	template <std::floating_point T>
	inline constexpr T fast_tanh(T x) noexcept
		{ return x * (T(27) + x * x) / (T(27) + T(9) * x * x); }

	// Simple mirror folding function with repeating peaks
	template<typename T>
		requires (std::is_unsigned_v<T>)
	inline constexpr T peak_mirror_fold(T value, std::size_t max) noexcept {
		auto wrapped{ value % (max * 2) };
		return T(wrapped < max ? wrapped : (2 * max - 1) - wrapped);
	}
}

/*==================================================================*/

namespace EzMaths {
	inline constexpr u8 fixedMul8(u8 x, u8 y) noexcept {
		return u8(((x * (y | y << 8)) + 0x8080u) >> 16);
	}

	inline constexpr u8 fixedScale8(u8 x, u16 y) noexcept {
		return u8(std::min((x * y * 257u + 0x8080u) >> 16, 255u));
	}

	inline constexpr u8 fixedLerp8(u8 x, u8 y, Weight w) noexcept {
		return u8(fixedMul8(x, u8(255 - w)) + fixedMul8(y, w));
	}

	template <std::integral T>
	inline constexpr T fixedLerpN(T x, T y, Weight w, T full_hue, T half_hue) noexcept {
		const auto shortest{ (y - x + half_hue) % full_hue - half_hue };
		return T((x + T(shortest * w.as_fp()) + full_hue) % full_hue);
	}
}

/*==================================================================*/

namespace EzMaths {
	inline constexpr u32 bitDup8(u32 data) noexcept {
		data = (data << 4 | data) & 0x0F0Fu;
		data = (data << 2 | data) & 0x3333u;
		data = (data << 1 | data) & 0x5555u;
		return (data << 1 | data) & 0xFFFFu;
	}

	inline constexpr u32 bitDup16(u32 data) noexcept {
		data = (data << 8 | data) & 0x00FF00FFu;
		data = (data << 4 | data) & 0x0F0F0F0Fu;
		data = (data << 2 | data) & 0x33333333u;
		data = (data << 1 | data) & 0x55555555u;
		return  data << 1 | data;
	}

	inline constexpr u64 bitDup32(u64 data) noexcept {
		data = (data << 16 | data) & 0x0000FFFF0000FFFFu;
		data = (data <<  8 | data) & 0x00FF00FF00FF00FFu;
		data = (data <<  4 | data) & 0x0F0F0F0F0F0F0F0Fu;
		data = (data <<  2 | data) & 0x3333333333333333u;
		data = (data <<  1 | data) & 0x5555555555555555u;
		return  data <<  1 | data;
	}
}

/*==================================================================*/

PRECISE_FP_BEGIN
namespace EzMaths{
	class EMA {
		f32 alpha{};
		f32 value{};

	public:
		constexpr EMA() : value{ std::numeric_limits<f32>::quiet_NaN() } {}

		constexpr void set_alpha(f32 v) noexcept
			{ alpha = 2.0f / std::max(v, 0.0f); }

		constexpr void add(f32 v) noexcept {
			if (value != value) [[unlikely]] { value = v; }
			else { value = alpha * v + (1 - alpha) * value; }
		}

		constexpr f32 avg() const noexcept { return value; }
	};
}
PRECISE_FP_END

/*==================================================================*/

namespace ez = EzMaths;
