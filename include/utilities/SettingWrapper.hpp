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
using PtrVariantFor = std::variant<T*...>;

// Types TOML supports explicitly
using SettingVariant = PtrVariantFor<
	std::int8_t,  std::int16_t,  std::int32_t,  std::int64_t,
	std::uint8_t, std::uint16_t, std::uint32_t,
	bool, float, double, std::string
>;

template <typename T, typename Variant, std::size_t... I>
consteval bool is_invocable_over_variant_(std::index_sequence<I...>) noexcept \
	{ return (std::invocable<T, std::variant_alternative_t<I, Variant>> && ...); }

template<typename T, typename Variant>
concept SettingVisitor = is_invocable_over_variant_<T, Variant> \
	(std::make_index_sequence<std::variant_size_v<Variant>>{});

static_assert(SettingVisitor<decltype([](auto*) noexcept {}), SettingVariant>,
	"Visitor method must support invocation with all types of SettingVariant!");

/*==================================================================*/

class SettingWrapper {
	SettingVariant m_variant;
	std::size_t m_elem_count;

public:
	template <typename T>
	SettingWrapper(T* ptr, std::size_t elem_count) noexcept
		: m_variant(ptr), m_elem_count(elem_count ? elem_count : 1)
	{}

	auto elem_count() const noexcept { return m_elem_count; }

	template <typename T>
	void set(T&& value) noexcept {
		std::visit([&](auto* ptr) noexcept {
			using PtrT = std::decay_t<decltype(*ptr)>;
			if constexpr (std::is_assignable_v<PtrT&, T&&>) \
				{ *ptr = std::forward<T>(value); }
		}, m_variant);
	}

	template <typename T>
	T get(T fallback = T{}) const noexcept {
		return std::visit([&](auto* ptr) noexcept -> T {
			using PtrT = std::decay_t<decltype(*ptr)>;
			if constexpr (std::is_convertible_v<PtrT, T>) \
				{ return static_cast<T>(*ptr); }
			return fallback;
		}, m_variant);
	}

	void visit(SettingVisitor<SettingVariant> auto&& visitor) noexcept {
		std::visit(std::forward<decltype(visitor)>(visitor), m_variant); }

	void visit(SettingVisitor<SettingVariant> auto&& visitor) const noexcept {
		std::visit(std::forward<decltype(visitor)>(visitor), m_variant); }
};

/*==================================================================*/

using SettingsMap = std::unordered_map<std::string, SettingWrapper>;

template<typename T, typename Variant>
concept VariantCompatible = requires { Variant(std::in_place_type<T>); };

template <typename T> requires (VariantCompatible<T*, SettingVariant>)
inline auto make_setting_link(const std::string& key, T* const ptr, std::size_t elem_count = 1) noexcept {
	return std::pair(key, SettingWrapper(ptr, elem_count));
}
