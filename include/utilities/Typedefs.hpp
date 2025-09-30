/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <cstdint>
#include <cstddef>

#include <filesystem>
#include <string_view>

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

using Str  = std::string;
using StrV = std::string_view;
using Path = std::filesystem::path;

using namespace std::string_literals;
using namespace std::string_view_literals;

inline constexpr auto KiB(size_type n) noexcept { return 1024ull * n; }
inline constexpr auto MiB(size_type n) noexcept { return 1024ull * KiB(n); }
inline constexpr auto GiB(size_type n) noexcept { return 1024ull * MiB(n); }
