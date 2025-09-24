/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "Waveforms.hpp"
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
	using Weight = ez::Weight;

	static constexpr u8 Opaque{ 0xFF };
	static constexpr u8 Transparent{ 0x00 };

	u8 R{}, G{}, B{}, A{};

	constexpr RGBA() noexcept = default;
	constexpr RGBA(Packed color) noexcept
		: R{ u8(color >> 24) }
		, G{ u8(color >> 16) }
		, B{ u8(color >>  8) }
		, A{ u8(color >>  0) }
	{}
	constexpr RGBA(u8 R, u8 G, u8 B, u8 A = Opaque) noexcept
		: R{ R }, G{ G }, B{ B }, A{ A }
	{}

	constexpr Packed RGB_()     const noexcept { return R << 24 | G << 16 | B << 8 | 0; }
	constexpr Packed RBG_()     const noexcept { return R << 24 | B << 16 | G << 8 | 0; }
	constexpr Packed GRB_()     const noexcept { return G << 24 | R << 16 | B << 8 | 0; }
	constexpr Packed GBR_()     const noexcept { return G << 24 | B << 16 | R << 8 | 0; }
	constexpr Packed BRG_()     const noexcept { return B << 24 | R << 16 | G << 8 | 0; }
	constexpr Packed BGR_()     const noexcept { return B << 24 | G << 16 | R << 8 | 0; }

	constexpr Packed RBGA()     const noexcept { return R << 24 | B << 16 | G << 8 | A; }
	constexpr Packed GRBA()     const noexcept { return G << 24 | R << 16 | B << 8 | A; }
	constexpr Packed GBRA()     const noexcept { return G << 24 | B << 16 | R << 8 | A; }
	constexpr Packed BRGA()     const noexcept { return B << 24 | R << 16 | G << 8 | A; }
	constexpr Packed BGRA()     const noexcept { return B << 24 | G << 16 | R << 8 | A; }
	constexpr operator Packed() const noexcept { return R << 24 | G << 16 | B << 8 | A; }

	static constexpr RGBA lerp(RGBA x, RGBA y, Weight w) noexcept {
		return RGBA(
			ez::fixedLerp8(x.R, y.R, w),
			ez::fixedLerp8(x.G, y.G, w),
			ez::fixedLerp8(x.B, y.B, w),
			ez::fixedLerp8(x.A, y.A, w));
	}

	class Blend {
		// Shortcut cast to invert a channel value
		template <std::integral T>
		[[nodiscard]] static constexpr
		auto x8(T x) noexcept { return u8(~x); }

		static constexpr auto MIN{   0 };
		static constexpr auto MAX{ 255 };

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
			{ return x8(ez::fixedMul8(x8(src), x8(dst))); }

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
			{ return ez::fixedMul8(src, dst); }

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
				? u8(ez::fixedMul8(   src,     dst)  * 2)
				: x8(ez::fixedMul8(x8(src), x8(dst)) * 2);
		}

		[[nodiscard]] static constexpr
		u8 Glow(u8 src, u8 dst) noexcept
			{ return u8(dst == MAX ? MAX : std::min((ez::fixedMul8(src, dst) * MAX) / x8(dst), MAX)); }

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
	RGBA channelBlend(RGBA src, RGBA dst, BlendFunc func) noexcept {
		return RGBA{
			func(src.R, dst.R),
			func(src.G, dst.G),
			func(src.B, dst.B)
		};
	}

	/**
	 * @brief Alpha blends two RGBA colors together.
	 * @param[in] src :: Source color.
	 * @param[in] dst :: Destination color.
	 */
	[[nodiscard]] static constexpr
	RGBA blendAlpha(RGBA src, RGBA dst) noexcept {
		switch (src.A) {
			case Opaque:      return src;
			case Transparent: return dst;
			default:          return lerp(dst, src, src.A);
		}
	}

	/**
	 * @brief Alpha blends two RGBA colors together with custom weight.
	 * @param[in] src :: Source color.
	 * @param[in] dst :: Destination color.
	 * @param[in] weight :: Custom weight bias.
	 */
	[[nodiscard]] static constexpr
	RGBA blendWeightedAlpha(RGBA src, RGBA dst, Weight weight) noexcept {
		switch (weight) {
			case Opaque:      return src;
			case Transparent: return dst;
			default:          return lerp(dst, src, weight);
		}
	}

	/**
	 * @brief Blends two RGBA colors together using the provided BlendFunc(), then applies alphaBlend().
	 * @param[in] src :: Source color.
	 * @param[in] dst :: Destination color.
	 * @param[in] func :: Function to blend each channel, but not alpha.
	 * @param[in] weight :: Custom weight bias. If 0, default to source alpha.
	 */
	[[nodiscard]] static constexpr
	RGBA compositeBlend(RGBA src, RGBA dst, BlendFunc func, Weight weight = Opaque) noexcept {
		if (auto alpha{ ez::fixedMul8(src.A, weight) }) [[likely]]
			{ return blendWeightedAlpha(channelBlend(src, dst, func), dst, alpha); }
		return dst;
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
	using Weight = ez::Weight;

	static constexpr auto full_hue{ Type_H(0x600u) };
	static constexpr auto half_hue{ Type_H(full_hue >> 1) };

	Type_H H{};
	Type_S S{};
	Type_V V{};

	constexpr HSV() noexcept = default;
	constexpr HSV(Packed color) noexcept
		: H{ Type_H(color >> 16) }
		, S{ Type_S(color >>  8) }
		, V{ Type_V(color >>  0) }
	{}
	constexpr HSV(Type_H H, Type_S S, Type_V V) noexcept
		: H{ H }, S{ S }, V{ V }
	{}

	constexpr operator Packed() const noexcept { return Packed(H) << 16 | S << 8 | V; }

	static constexpr HSV lerp(HSV x, HSV y, Weight w) noexcept {
		return HSV(
			ez::fixedLerpN(x.H, y.H, w, full_hue, half_hue),
			ez::fixedLerp8(x.S, y.S, w),
			ez::fixedLerp8(x.V, y.V, w));
	}
};

/*==================================================================*/

struct OKLAB {
	using Type_F = ez::f64;
	using Weight = ez::Weight;

	Type_F L{}, A{}, B{};

	constexpr OKLAB() noexcept = default;
	constexpr OKLAB(Type_F L, Type_F A, Type_F B) noexcept
		: L{ L }, A{ A }, B{ B }
	{}

	static CONSTEXPR_MATH Type_F gamma_def(Type_F x) noexcept {
		return x <= 0.0404500 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
		//return std::pow(x, 2.2); // quick path
	}
	static CONSTEXPR_MATH Type_F gamma_inv(Type_F x) noexcept {
		return x <= 0.0031308 ? x * 12.92 : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
		//return std::pow(x, 1.0 / 2.2); // quick path
	}

	static constexpr OKLAB lerp(OKLAB x, OKLAB y, Weight w) noexcept {
		return OKLAB(
			std::lerp(x.L, y.L, w.as_fp()),
			std::lerp(x.A, y.A, w.as_fp()),
			std::lerp(x.B, y.B, w.as_fp()));
	}

	static CONSTEXPR_MATH RGBA lerp(RGBA x, RGBA y, Weight w) noexcept {
		return ::to_RGBA(lerp(::to_OKLAB(x), ::to_OKLAB(y), w));
	}
};

struct OKLCH {
	using Type_F = OKLAB::Type_F;
	using Weight = OKLAB::Weight;

	Type_F L{}, C{}, H{};

	constexpr OKLCH() noexcept = default;
	constexpr OKLCH(Type_F L, Type_F C, Type_F H) noexcept
		: L{ L }, C{ C }, H{ H }
	{}

	static constexpr OKLCH lerp(OKLCH x, OKLCH y, Weight w) noexcept {
		const auto delta{ ez::fmod(y.H - x.H + \
			std::numbers::pi * 3, std::numbers::pi * 2) - std::numbers::pi };
		return OKLCH(
			std::lerp(x.L, y.L, w.as_fp()),
			std::lerp(x.C, y.C, w.as_fp()),
			y.H + delta * w);
	}

	static CONSTEXPR_MATH OKLAB lerp(OKLAB x, OKLAB y, Weight w) noexcept {
		return ::to_OKLAB(lerp(::to_OKLCH(x), ::to_OKLCH(y), w));
	}

	static CONSTEXPR_MATH RGBA lerp(RGBA x, RGBA y, Weight w) noexcept {
		return ::to_RGBA(lerp(::to_OKLCH(x), ::to_OKLCH(y), w));
	}
};

/*==================================================================*/

inline constexpr HSV to_HSV(RGBA in) noexcept {
	const auto maxV{ std::max({ in.R, in.G, in.B }) };
	const auto minV{ std::min({ in.R, in.G, in.B }) };
	const auto diff{ maxV - minV };

	if (diff == 0)
		{ return HSV(0, 0, maxV); }

	auto hueV{ 0 };

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

	const auto hueV{ in.H & 0xFF };

	const auto valP{ RGBA::u8((in.V * (0x00FF - in.S)                  + 0x007F) / 0x00FF) };
	const auto valQ{ RGBA::u8((in.V * (0xFF00 - in.S *          hueV)  + 0x7FFF) / 0xFF00) };
	const auto valT{ RGBA::u8((in.V * (0xFF00 - in.S * (0x100 - hueV)) + 0x7FFF) / 0xFF00) };

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
	const auto R{ OKLAB::gamma_def(in.R / 255.0f) };
	const auto G{ OKLAB::gamma_def(in.G / 255.0f) };
	const auto B{ OKLAB::gamma_def(in.B / 255.0f) };

	const auto L{ std::cbrt(0.4122214708 * R + 0.5363325363 * G + 0.0514459929 * B) };
	const auto M{ std::cbrt(0.2119034982 * R + 0.6806995451 * G + 0.1073969566 * B) };
	const auto S{ std::cbrt(0.0883024619 * R + 0.2817188376 * G + 0.6299787005 * B) };

	return OKLAB(
		0.2104542553 * L + 0.7936177850 * M - 0.0040720468 * S,
		1.9779984951 * L - 2.4285922050 * M + 0.4505937099 * S,
		0.0259040371 * L + 0.7827717662 * M - 0.8086757660 * S
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
	const auto L{ std::pow(in.L + in.A * 0.39633778 + in.B * 0.21580376, 3.0) };
	const auto M{ std::pow(in.L - in.A * 0.10556113 - in.B * 0.06385417, 3.0) };
	const auto S{ std::pow(in.L - in.A * 0.08948418 - in.B * 1.29148554, 3.0) };

	const auto R{ +4.07674 * L - 3.30771 * M + 0.23097 * S };
	const auto G{ -1.26844 * L + 2.60976 * M - 0.34132 * S };
	const auto B{ -0.00439 * L - 0.70342 * M + 1.70758 * S };

	return RGBA(
		RGBA::u8(ez::round(255.0 * OKLAB::gamma_inv(R))),
		RGBA::u8(ez::round(255.0 * OKLAB::gamma_inv(G))),
		RGBA::u8(ez::round(255.0 * OKLAB::gamma_inv(B)))
	);
}

inline CONSTEXPR_MATH RGBA to_RGBA(OKLCH in) noexcept {
	return ::to_RGBA(::to_OKLAB(in));
}
