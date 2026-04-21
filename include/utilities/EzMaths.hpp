/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <concepts>
#include <algorithm>
#include <limits>
#include <bit>

#include "Macros.hpp"

/*==================================================================*/

using f64 = double;
using f32 = float;

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

using s64 = std::int64_t;
using s32 = std::int32_t;
using s16 = std::int16_t;
using s8  = std::int8_t;

inline constexpr auto KiB(unsigned long long v) noexcept { return 1024ull * v; }
inline constexpr auto MiB(unsigned long long v) noexcept { return 1024ull * KiB(v); }
inline constexpr auto GiB(unsigned long long v) noexcept { return 1024ull * MiB(v); }

inline constexpr auto operator""_KiB(unsigned long long v) noexcept { return KiB(v); }
inline constexpr auto operator""_MiB(unsigned long long v) noexcept { return MiB(v); }
inline constexpr auto operator""_GiB(unsigned long long v) noexcept { return GiB(v); }

/*==================================================================*/

namespace EzMaths {
	struct alignas(sizeof(s32) * 2) Point {
		s32 x{}, y{};

		constexpr Point(s32 x = 0, s32 y = 0) noexcept
			: x(x), y(y)
		{}

		constexpr auto operator+(const Point& other) const noexcept
			{ return Point(x + other.x, y + other.y); }
	};

	struct alignas(sizeof(s32) * 2) Frame {
		s32 w{}, h{};

		constexpr Frame(s32 w = 0, s32 h = 0) noexcept
			: w(w < 0 ? 0 : w)
			, h(h < 0 ? 0 : h)
		{}

		constexpr auto area() const noexcept { return u64(w * h); }
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
		u8 m_weight{};

	public:
		template <std::integral Int>
		constexpr Weight(Int value) noexcept : m_weight(u8(value)) {}
		constexpr Weight(f64 value) noexcept : m_weight(u8(value * 255.0)) {}

		// Cast weight to floating-point [0..1] value
		constexpr auto as_fp() const noexcept {
			return (1.0f / 255.0f) * m_weight;
		}

		constexpr operator u8() const noexcept { return m_weight; }
	};
}

/*==================================================================*/

namespace EzMaths {
	inline constexpr auto intersect(const Rect& lhs, const Rect& rhs) noexcept {
		auto x1 = std::max(lhs.x, rhs.x);
		auto y1 = std::max(lhs.y, rhs.y);
		auto x2 = std::min(lhs.x + lhs.w, rhs.x + rhs.w);
		auto y2 = std::min(lhs.y + lhs.h, rhs.y + rhs.h);

		auto w = std::max(0, x2 - x1);
		auto h = std::max(0, y2 - y1);
		auto x = w > 0 ? x1 : 0;
		auto y = h > 0 ? y1 : 0;

		return Rect(x, y, w, h);
	}

	inline constexpr auto distance(const Point& lhs, const Point& rhs) noexcept {
		s64 dx = lhs.x - rhs.x;
		s64 dy = lhs.y - rhs.y;
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
	template <typename T>
		requires (std::is_unsigned_v<T>)
	inline constexpr T peak_mirror_fold(T value, u64 max) noexcept {
		auto wrapped = value % (max * 2);
		return T(wrapped < max ? wrapped : (2 * max - 1) - wrapped);
	}

	template <std::floating_point T>
	constexpr bool isfinite(T value) noexcept {
		#ifdef __cpp_lib_constexpr_cmath // C++23
			return std::isfinite(value);
		#else
			if constexpr (sizeof(T) == sizeof(u64)) {
				static constexpr u64 exponent_mask = 0x7FF0000000000000;
				return (std::bit_cast<u64>(value) & exponent_mask) != exponent_mask;
			} else if constexpr (sizeof(T) == sizeof(u32)) {
				static constexpr u32 exponent_mask = 0x7F800000;
				return (std::bit_cast<u32>(value) & exponent_mask) != exponent_mask;
			} else {
				static_assert(sizeof(T) == 0, "isfinite: unsupported float type");
			}
		#endif
	}
}

/*==================================================================*/

namespace EzMaths {
	inline constexpr u8 fixed_mul8(u8 x, u8 y) noexcept {
		return u8(((x * (y | y << 8)) + 0x8080u) >> 16);
	}

	inline constexpr u8 fixed_scale8(u8 x, u16 y) noexcept {
		return u8(std::min((x * y * 257u + 0x8080u) >> 16, 255u));
	}

	inline constexpr u8 fixed_lerp8(u8 x, u8 y, Weight w) noexcept {
		return u8(fixed_mul8(x, u8(255 - w)) + fixed_mul8(y, w));
	}

	template <std::integral T>
	inline constexpr T fixed_lerpN(T x, T y, Weight w, T full_hue, T half_hue) noexcept {
		const auto shortest = (y - x + half_hue) % full_hue - half_hue;
		return T((x + T(shortest * w.as_fp()) + full_hue) % full_hue);
	}
}

/*==================================================================*/

namespace EzMaths {
	inline constexpr u32 bit_dup8(u32 data) noexcept {
		data = (data << 4 | data) & 0x0F0Fu;
		data = (data << 2 | data) & 0x3333u;
		data = (data << 1 | data) & 0x5555u;
		return (data << 1 | data) & 0xFFFFu;
	}

	inline constexpr u32 bit_dup16(u32 data) noexcept {
		data = (data << 8 | data) & 0x00FF00FFu;
		data = (data << 4 | data) & 0x0F0F0F0Fu;
		data = (data << 2 | data) & 0x33333333u;
		data = (data << 1 | data) & 0x55555555u;
		return  data << 1 | data;
	}

	inline constexpr u64 bit_dup32(u64 data) noexcept {
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
		constexpr EMA() : value(std::numeric_limits<f32>::quiet_NaN()) {}

		constexpr void set_alpha(f32 v) noexcept {
			alpha = v > 0.0f ? std::min(2.0f / v, 1.0f) : 1.0f;
		}

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
