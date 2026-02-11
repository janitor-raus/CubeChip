/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <numbers>
#include <cmath>
#include <bit>

#include "EzMaths.hpp"

/*==================================================================*/

struct RGBA;
struct HSV;
struct OKLAB;
struct OKLCH;

inline constexpr      HSV   to_HSV  (RGBA  in) noexcept;
inline constexpr      RGBA  to_RGBA (HSV   in) noexcept;
inline CONSTEXPR_MATH OKLAB to_OKLAB(RGBA  in) noexcept;
inline CONSTEXPR_MATH OKLAB to_OKLAB(OKLCH in) noexcept;
inline CONSTEXPR_MATH OKLCH to_OKLCH(OKLAB in) noexcept;
inline CONSTEXPR_MATH OKLCH to_OKLCH(RGBA  in) noexcept;
inline CONSTEXPR_MATH RGBA  to_RGBA (OKLAB in) noexcept;
inline CONSTEXPR_MATH RGBA  to_RGBA (OKLCH in) noexcept;

/*==================================================================*/

struct alignas(4) RGBA {
	using u8     = ez::u8;
	using Packed = ez::u32;

	static constexpr u8 Opaque_A      = 0xFF;
	static constexpr u8 Transparent_A = 0x00;

	static constexpr Packed Transparent = 0x00000000;
	static constexpr Packed Black       = 0x000000FF;
	static constexpr Packed White       = 0xFFFFFFFF;
	static constexpr Packed Red         = 0xFF0000FF;
	static constexpr Packed Green       = 0x00FF00FF;
	static constexpr Packed Blue        = 0x0000FFFF;
	static constexpr Packed Yellow      = 0xFFFF00FF;
	static constexpr Packed Cyan        = 0x00FFFFFF;
	static constexpr Packed Magenta     = 0xFF00FFFF;

	static constexpr Packed min = 0x00000000;
	static constexpr Packed max = 0xFFFFFFFF;

	u8 A{}, B{}, G{}, R{};

	constexpr RGBA() noexcept = default;
	constexpr RGBA(Packed color) noexcept
		: A(u8(color >>  0))
		, B(u8(color >>  8))
		, G(u8(color >> 16))
		, R(u8(color >> 24))
	{}
	constexpr RGBA(u8 R, u8 G, u8 B, u8 A = Opaque_A) noexcept
		: A(A), B(B), G(G), R(R)
	{}

private:
	constexpr Packed pack(u8 c1, u8 c2, u8 c3, u8 c4) const noexcept {
		return (Packed(c1) << 24) | (Packed(c2) << 16) | (Packed(c3) << 8) | Packed(c4);
	}

public:
	constexpr RGBA& set_R(u8 r) noexcept { R = r; return *this; }
	constexpr RGBA& set_G(u8 g) noexcept { G = g; return *this; }
	constexpr RGBA& set_B(u8 b) noexcept { B = b; return *this; }
	constexpr RGBA& set_A(u8 a) noexcept { A = a; return *this; }

	constexpr Packed raw()      const noexcept { return std::bit_cast<Packed>(*this); }
	constexpr operator Packed() const noexcept { return raw(); }

	constexpr Packed XRGB()     const noexcept { return pack(Opaque_A, R, G, B); }
	constexpr Packed XRBG()     const noexcept { return pack(Opaque_A, R, B, G); }
	constexpr Packed XGRB()     const noexcept { return pack(Opaque_A, G, R, B); }
	constexpr Packed XGBR()     const noexcept { return pack(Opaque_A, G, B, R); }
	constexpr Packed XBRG()     const noexcept { return pack(Opaque_A, B, R, G); }
	constexpr Packed XBGR()     const noexcept { return pack(Opaque_A, B, G, R); }

	constexpr Packed RGBX()     const noexcept { return pack(R, G, B, Opaque_A); }
	constexpr Packed RBGX()     const noexcept { return pack(R, B, G, Opaque_A); }
	constexpr Packed GRBX()     const noexcept { return pack(G, R, B, Opaque_A); }
	constexpr Packed GBRX()     const noexcept { return pack(G, B, R, Opaque_A); }
	constexpr Packed BRGX()     const noexcept { return pack(B, R, G, Opaque_A); }
	constexpr Packed BGRX()     const noexcept { return pack(B, G, R, Opaque_A); }

	constexpr Packed ARGB()     const noexcept { return pack(A, R, G, B); }
	constexpr Packed ARBG()     const noexcept { return pack(A, R, B, G); }
	constexpr Packed AGRB()     const noexcept { return pack(A, G, R, B); }
	constexpr Packed AGBR()     const noexcept { return pack(A, G, B, R); }
	constexpr Packed ABRG()     const noexcept { return pack(A, B, R, G); }
	constexpr Packed ABGR()     const noexcept { return pack(A, B, G, R); }

	constexpr Packed RBGA()     const noexcept { return pack(R, B, G, A); }
	constexpr Packed GRBA()     const noexcept { return pack(G, R, B, A); }
	constexpr Packed GBRA()     const noexcept { return pack(G, B, R, A); }
	constexpr Packed BRGA()     const noexcept { return pack(B, R, G, A); }
	constexpr Packed BGRA()     const noexcept { return pack(B, G, R, A); }

	static constexpr RGBA lerp(RGBA x, RGBA y, ez::Weight w) noexcept {
		return RGBA(
			ez::fixed_lerp8(x.R, y.R, w),
			ez::fixed_lerp8(x.G, y.G, w),
			ez::fixed_lerp8(x.B, y.B, w),
			ez::fixed_lerp8(x.A, y.A, w));
	}

	class Blend {
		// Shortcut cast to invert a channel value
		template <std::integral T>
		[[nodiscard]] static constexpr
		auto x8(T x) noexcept { return u8(~x); }

		static constexpr auto MIN =   0;
		static constexpr auto MAX = 255;

	public:
		[[nodiscard]] static constexpr
		u8 None(u8 src, u8) noexcept
			{ return src; }

		/*------------------------ LIGHTENING MODES ------------------------*/

		[[nodiscard]] static constexpr
		u8 Lighten(u8 src, u8 dst) noexcept
			{ return std::max(src, dst); }

		[[nodiscard]] static constexpr
		u8 Screen(u8 src, u8 dst) noexcept
			{ return x8(ez::fixed_mul8(x8(src), x8(dst))); }

		[[nodiscard]] static constexpr
		u8 ColorDodge(u8 src, u8 dst) noexcept
			{ return u8(src == MAX ? MAX : std::min((dst * MAX) / x8(src), MAX)); }

		[[nodiscard]] static constexpr
		u8 LinearDodge(u8 src, u8 dst) noexcept
			{ return u8(std::min(src + dst, MAX)); }

		/*------------------------ DARKENING MODES -------------------------*/

		[[nodiscard]] static constexpr
		u8 Darken(u8 src, u8 dst) noexcept
			{ return std::min(src, dst); }

		[[nodiscard]] static constexpr
		u8 Multiply(u8 src, u8 dst) noexcept
			{ return ez::fixed_mul8(src, dst); }

		[[nodiscard]] static constexpr
		u8 ColorBurn(u8 src, u8 dst) noexcept
			{ return u8(src == MIN ? MIN : std::max(((src + dst - MAX) * MAX) / src, MIN)); }

		[[nodiscard]] static constexpr
		u8 LinearBurn(u8 src, u8 dst) noexcept
			{ return u8(std::max(src + dst - MAX, MIN)); }

		/*-------------------------- OTHER MODES ---------------------------*/

		[[nodiscard]] static constexpr
		u8 Average(u8 src, u8 dst) noexcept
			{ return u8((src + dst + 1) >> 1); }

		[[nodiscard]] static constexpr
		u8 Difference(u8 src, u8 dst) noexcept
			{ return u8(ez::abs(src - dst)); }

		[[nodiscard]] static constexpr
		u8 Negation(u8 src, u8 dst) noexcept
			{ return x8(ez::abs(MAX - (src + dst))); }

		[[nodiscard]] static constexpr
		u8 Overlay(u8 src, u8 dst) noexcept {
			return src < 128
				? u8(ez::fixed_mul8(   src,     dst)  * 2)
				: x8(ez::fixed_mul8(x8(src), x8(dst)) * 2);
		}

		[[nodiscard]] static constexpr
		u8 Glow(u8 src, u8 dst) noexcept
			{ return u8(dst == MAX ? MAX : std::min((ez::fixed_mul8(src, dst) * MAX) / x8(dst), MAX)); }

		[[nodiscard]] static constexpr
		u8 Reflect(u8 src, u8 dst) noexcept
			{ return Glow(dst, src); }
	};

	// Channel blend function type alias
	using BlendFunc = u8(*)(u8 src, u8 dst) noexcept;

	/**
	 * @brief Blends two RGBA colors together using the provided BlendFunc().
	 * @param[in] src :: Source color.
	 * @param[in] dst :: Destination color.
	 * @param[in] func :: Function to blend each channel, but not alpha.
	 */
	[[nodiscard]] static constexpr
	RGBA channel_blend(RGBA src, RGBA dst, BlendFunc func) noexcept {
		return RGBA(
			func(src.R, dst.R),
			func(src.G, dst.G),
			func(src.B, dst.B)
		);
	}

	/**
	 * @brief Alpha blends two RGBA colors together.
	 * @param[in] src :: Source color.
	 * @param[in] dst :: Destination color.
	 */
	[[nodiscard]] static constexpr
	RGBA alpha_blend(RGBA src, RGBA dst) noexcept {
		switch (src.A) {
			case Opaque_A:      return src;
			case Transparent_A: return dst;
			default:            return lerp(dst, src, src.A);
		}
	}

	/**
	 * @brief Alpha blends two RGBA colors together with custom source weight.
	 * @param[in] src :: Source color.
	 * @param[in] dst :: Destination color.
	 * @param[in] weight :: Custom source weight bias.
	 */
	[[nodiscard]] static constexpr
	RGBA weighted_alpha_blend(RGBA src, RGBA dst, ez::Weight weight) noexcept {
		switch (weight) {
			case Opaque_A:      return src;
			case Transparent_A: return dst;
			default:            return lerp(dst, src, weight);
		}
	}

	/**
	 * @brief Blends two RGBA colors together using the provided BlendFunc(), then applies alpha_blend().
	 * @param[in] src :: Source color.
	 * @param[in] dst :: Destination color.
	 * @param[in] func :: Function to blend each channel, but not alpha.
	 * @param[in] weight :: Custom source weight bias. If 0, default to source alpha.
	 */
	[[nodiscard]] static constexpr
	RGBA composite_blend(RGBA src, RGBA dst, BlendFunc func, ez::Weight weight = Opaque_A) noexcept {
		if (auto alpha = ez::fixed_mul8(src.A, weight)) [[likely]] {
			return weighted_alpha_blend(channel_blend(src, dst, func), dst, alpha);
		}
		return dst;
	}

	/**
	 * @brief Pre-multiplies an RGBA color by a given weight.
	 * @param[in] src :: Source color (alpha is ignored).
	 * @param[in] weight :: ez::Weight to pre-multiply by.
	 */
	[[nodiscard]] static constexpr
	RGBA premul(RGBA src, ez::Weight weight) noexcept {
		return RGBA(
			ez::fixed_mul8(src.R, weight),
			ez::fixed_mul8(src.G, weight),
			ez::fixed_mul8(src.B, weight),
			weight
		);
	}

	/**
	 * @brief Pre-multiplies an RGBA color with its own alpha as weight.
	 * @param[in] src :: Source color with alpha.
	 */
	[[nodiscard]] static constexpr
	RGBA premul(RGBA src) noexcept {
		return RGBA(
			ez::fixed_mul8(src.R, src.A),
			ez::fixed_mul8(src.G, src.A),
			ez::fixed_mul8(src.B, src.A),
			src.A
		);
	}
};

// Shifts an (A)RGB color to RGBA color
constexpr RGBA operator""_rgb(unsigned long long value) noexcept
	{ return RGBA::Packed(value << 8); }


/*==================================================================*/

struct alignas(4) HSV {
	using Type_H = ez::s16;
	using Type_S = ez::u8;
	using Type_V = Type_S;
	using Packed = ez::u32;

	static constexpr auto full_hue = Type_H(0x600u);
	static constexpr auto half_hue = Type_H(full_hue >> 1);

	Type_H H{};
	Type_S S{};
	Type_V V{};

	constexpr HSV() noexcept = default;
	constexpr HSV(Packed color) noexcept
		: H(Type_H(color >> 16))
		, S(Type_S(color >>  8))
		, V(Type_V(color >>  0))
	{}
	constexpr HSV(Type_H H, Type_S S, Type_V V) noexcept
		: H(H), S(S), V(V)
	{}

	constexpr operator Packed() const noexcept { return Packed(H) << 16 | S << 8 | V; }

	static constexpr HSV lerp(HSV x, HSV y, ez::Weight w) noexcept {
		return HSV(
			ez::fixed_lerpN(x.H, y.H, w, full_hue, half_hue),
			ez::fixed_lerp8(x.S, y.S, w),
			ez::fixed_lerp8(x.V, y.V, w));
	}
};

/*==================================================================*/

struct OKLAB {
	using Type_F = ez::f32;

	Type_F L{}, A{}, B{};

	constexpr OKLAB() noexcept = default;
	constexpr OKLAB(Type_F L, Type_F A, Type_F B) noexcept
		: L(L), A(A), B(B)
	{}

	static CONSTEXPR_MATH Type_F gamma_def(Type_F x) noexcept {
		return x <= 0.0404500f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
		//return std::pow(x, 2.2); // quick path
	}
	static CONSTEXPR_MATH Type_F gamma_inv(Type_F x) noexcept {
		return x <= 0.0031308f ? x * 12.92f : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
		//return std::pow(x, 1.0f / 2.2f); // quick path
	}

	static constexpr OKLAB lerp(OKLAB x, OKLAB y, ez::Weight w) noexcept {
		return OKLAB(
			std::lerp(x.L, y.L, w.as_fp()),
			std::lerp(x.A, y.A, w.as_fp()),
			std::lerp(x.B, y.B, w.as_fp()));
	}

	static CONSTEXPR_MATH RGBA lerp(RGBA x, RGBA y, ez::Weight w) noexcept {
		return ::to_RGBA(lerp(::to_OKLAB(x), ::to_OKLAB(y), w));
	}
};

struct OKLCH {
	using Type_F = OKLAB::Type_F;

	Type_F L{}, C{}, H{};

	constexpr OKLCH() noexcept = default;
	constexpr OKLCH(Type_F L, Type_F C, Type_F H) noexcept
		: L(L), C(C), H(H)
	{}

	static constexpr OKLCH lerp(OKLCH x, OKLCH y, ez::Weight w) noexcept {
		const auto delta = ez::fmod(y.H - x.H + ez::f32(std::numbers::pi * 3),
			ez::f32(std::numbers::pi * 2)) - ez::f32(std::numbers::pi);
		return OKLCH(
			std::lerp(x.L, y.L, w.as_fp()),
			std::lerp(x.C, y.C, w.as_fp()),
			y.H + delta * w);
	}

	static CONSTEXPR_MATH OKLAB lerp(OKLAB x, OKLAB y, ez::Weight w) noexcept {
		return ::to_OKLAB(lerp(::to_OKLCH(x), ::to_OKLCH(y), w));
	}

	static CONSTEXPR_MATH RGBA lerp(RGBA x, RGBA y, ez::Weight w) noexcept {
		return ::to_RGBA(lerp(::to_OKLCH(x), ::to_OKLCH(y), w));
	}
};

/*==================================================================*/

inline constexpr HSV to_HSV(RGBA in) noexcept {
	const auto maxV = std::max({ in.R, in.G, in.B });
	const auto minV = std::min({ in.R, in.G, in.B });
	const auto diff = maxV - minV;

	if (diff == 0)
		{ return HSV(0, 0, maxV); }

	auto hueV = 0;

	/**/ if (maxV == in.R)
		{ hueV = 0x000 + ((in.G - in.B) * 0x100 / diff); }
	else if (maxV == in.G)
		{ hueV = 0x200 + ((in.B - in.R) * 0x100 / diff); }
	else if (maxV == in.B)
		{ hueV = 0x400 + ((in.R - in.G) * 0x100 / diff); }

	return HSV(HSV::Type_H((hueV + HSV::full_hue) % HSV::full_hue), \
		HSV::Type_S((diff * 0xFF + (maxV >> 1)) / maxV), HSV::Type_V(maxV));
}

inline constexpr RGBA to_RGBA(HSV in) noexcept {
	if (in.S == 0x00) [[unlikely]]
		{ return RGBA(in.V, in.V, in.V); }

	const auto hueV = in.H & 0xFF;

	const auto valP = RGBA::u8((in.V * (0x00FF - in.S)                  + 0x007F) / 0x00FF);
	const auto valQ = RGBA::u8((in.V * (0xFF00 - in.S *          hueV)  + 0x7FFF) / 0xFF00);
	const auto valT = RGBA::u8((in.V * (0xFF00 - in.S * (0x100 - hueV)) + 0x7FFF) / 0xFF00);

	switch (in.H >> 8) {
		case 0:  return RGBA(in.V, valT, valP);
		case 1:  return RGBA(valQ, in.V, valP);
		case 2:  return RGBA(valP, in.V, valT);
		case 3:  return RGBA(valP, valQ, in.V);
		case 4:  return RGBA(valT, valP, in.V);
		case 5:  return RGBA(in.V, valP, valQ);
		default: return RGBA(0, 0, 0);
	}
}

inline CONSTEXPR_MATH OKLAB to_OKLAB(RGBA in) noexcept {
	const auto R = OKLAB::gamma_def(in.R / 255.0f);
	const auto G = OKLAB::gamma_def(in.G / 255.0f);
	const auto B = OKLAB::gamma_def(in.B / 255.0f);

	const auto L = std::cbrt(0.4122214708f * R + 0.5363325363f * G + 0.0514459929f * B);
	const auto M = std::cbrt(0.2119034982f * R + 0.6806995451f * G + 0.1073969566f * B);
	const auto S = std::cbrt(0.0883024619f * R + 0.2817188376f * G + 0.6299787005f * B);

	return OKLAB(
		0.2104542553f * L + 0.7936177850f * M - 0.0040720468f * S,
		1.9779984951f * L - 2.4285922050f * M + 0.4505937099f * S,
		0.0259040371f * L + 0.7827717662f * M - 0.8086757660f * S
	);
}

inline CONSTEXPR_MATH OKLAB to_OKLAB(OKLCH in) noexcept {
	return OKLAB(in.L, in.C * std::cos(in.H), in.C * std::sin(in.H));
}

inline CONSTEXPR_MATH OKLCH to_OKLCH(OKLAB in) noexcept {
	return OKLCH(in.L, std::sqrt(in.A * in.A + in.B * in.B), std::atan2(in.B, in.A));
}

inline CONSTEXPR_MATH OKLCH to_OKLCH(RGBA in) noexcept {
	return ::to_OKLCH(::to_OKLAB(in));
}

inline CONSTEXPR_MATH RGBA to_RGBA(OKLAB in) noexcept {
	const auto L = std::pow(in.L + in.A * 0.39633778f + in.B * 0.21580376f, 3.0f);
	const auto M = std::pow(in.L - in.A * 0.10556113f - in.B * 0.06385417f, 3.0f);
	const auto S = std::pow(in.L - in.A * 0.08948418f - in.B * 1.29148554f, 3.0f);

	const auto R = +4.07674f * L - 3.30771f * M + 0.23097f * S;
	const auto G = -1.26844f * L + 2.60976f * M - 0.34132f * S;
	const auto B = -0.00439f * L - 0.70342f * M + 1.70758f * S;

	return RGBA(
		RGBA::u8(ez::round(255.0f * OKLAB::gamma_inv(R))),
		RGBA::u8(ez::round(255.0f * OKLAB::gamma_inv(G))),
		RGBA::u8(ez::round(255.0f * OKLAB::gamma_inv(B)))
	);
}

inline CONSTEXPR_MATH RGBA to_RGBA(OKLCH in) noexcept {
	return ::to_RGBA(::to_OKLAB(in));
}
