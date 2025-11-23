/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <string>
#include <variant>
#include <cstdint>
#include <cstddef>
#include <concepts>
#include <utility>
#include <type_traits>
#include <unordered_map>

/*==================================================================*/

template <typename... T>
using PtrVariant = std::variant<T*...>;

using SettingVar = PtrVariant<
	std::int8_t,  std::int16_t,  std::int32_t,  std::int64_t,
	std::uint8_t, std::uint16_t, std::uint32_t,
	bool, float, double, std::string
>;

template <typename T, typename Variant, std::size_t... I>
consteval bool is_invocable_over_variant_(std::index_sequence<I...>) noexcept
	{ return (std::invocable<T, std::variant_alternative_t<I, Variant>> && ...); }

template<typename T, typename Variant>
concept SettingVisitor = is_invocable_over_variant_<T, Variant> \
	(std::make_index_sequence<std::variant_size_v<Variant>>{});

static_assert(SettingVisitor<decltype([](auto*) {}), SettingVar>, "Visitor type mismatch!");

/*==================================================================*/

class SettingWrapper {
	SettingVar mSettingPtr;

public:
	template <typename T>
	SettingWrapper(T* ptr) noexcept
		: mSettingPtr(ptr)
	{}

	template <typename T>
	void set(T value) {
		std::visit([&value](auto* ptr) {
			using PtrT = std::decay_t<decltype(*ptr)>;
			if constexpr (std::is_assignable_v<PtrT&, T>)
				{ *ptr = std::forward<T>(value); }
		}, mSettingPtr);
	}

	template <typename T>
	T get(T value = T{}) const {
		return std::visit([&value](auto* ptr) -> T {
			using PtrT = std::decay_t<decltype(*ptr)>;
			if constexpr (std::is_convertible_v<PtrT, T>)
				{ return static_cast<T>(*ptr); }
			else
				{ return value; }
		}, mSettingPtr);
	}

	void visit(SettingVisitor<SettingVar> auto&& visitor)
		{ std::visit(std::forward<decltype(visitor)>(visitor), mSettingPtr); }

	void visit(SettingVisitor<SettingVar> auto&& visitor) const
		{ std::visit(std::forward<decltype(visitor)>(visitor), mSettingPtr); }
};

/*==================================================================*/

using SettingsMap = std::unordered_map<std::string, SettingWrapper>;

template<typename T, typename Variant>
concept VariantCompatible = requires
	{ Variant(std::in_place_type<T>); };

template <typename T>
	requires (VariantCompatible<T*, SettingVar>)
inline auto makeSetting(const std::string& key, T* const ptr) noexcept
	{ return std::pair(key, SettingWrapper(ptr)); }
