/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <concepts>
#include <algorithm>

/*==================================================================*/

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast(Dst& dst, Src&& src) noexcept
	{ dst = static_cast<Dst>(src); }

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_add(Dst& dst, Src&& src) noexcept
	{ dst += static_cast<Dst>(src); }

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_sub(Dst& dst, Src&& src) noexcept
	{ dst -= static_cast<Dst>(src); }

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_mul(Dst& dst, Src&& src) noexcept
	{ dst *= static_cast<Dst>(src); }

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_div(Dst& dst, Src&& src) noexcept
	{ dst /= static_cast<Dst>(src); }

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_mod(Dst& dst, Src&& src) noexcept
	{ dst %= static_cast<Dst>(src); }

/*==================================================================*/

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_rsub(Dst& dst, Src&& src) noexcept
	{ dst = static_cast<Dst>(src) - dst; }

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_rdiv(Dst& dst, Src&& src) noexcept
	{ dst = static_cast<Dst>(src) / dst; }

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_rmod(Dst& dst, Src&& src) noexcept
	{ dst = static_cast<Dst>(src) % dst; }

/*==================================================================*/

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_xor(Dst& dst, Src&& src) noexcept
	{ dst ^= static_cast<Dst>(src); }

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_and(Dst& dst, Src&& src) noexcept
	{ dst &= static_cast<Dst>(src); }

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_or(Dst& dst, Src&& src) noexcept
	{ dst |= static_cast<Dst>(src); }

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_shl(Dst& dst, Src&& src) noexcept
	{ dst <<= static_cast<Dst>(src); }

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_shr(Dst& dst, Src&& src) noexcept
	{ dst >>= static_cast<Dst>(src); }

/*==================================================================*/

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_rshl(Dst& dst, Src&& src) noexcept
	{ dst = static_cast<Dst>(src) << dst; }

template <std::integral Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_rshr(Dst& dst, Src&& src) noexcept
	{ dst = static_cast<Dst>(src) >> dst; }

/*==================================================================*/

template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_min(Dst& dst, Src&& src) noexcept
	{ dst = std::min(dst, static_cast<Dst>(src)); }


template <typename Dst, typename Src> requires (std::convertible_to<Src, Dst>)
inline constexpr void assign_cast_max(Dst& dst, Src&& src) noexcept
	{ dst = std::max(dst, static_cast<Dst>(src)); }

/*==================================================================*/
