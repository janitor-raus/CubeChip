/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>
#include <cstddef>

/*==================================================================*/

using f64 = double;
using f32 = float;

using size_type       = std::size_t;
using difference_type = std::ptrdiff_t;

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
