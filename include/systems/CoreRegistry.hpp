/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <span>
#include <memory>
#include <string_view>
#include <algorithm>

/*==================================================================*/

// DEPRECTATED, FOR FUTURE REFERENCE ONLY
enum class GameFileType {
	c2x, // CHIP-8X 2-page
	c4x, // CHIP-8X 4-page
	c8x, // CHIP-8X
	c8e, // CHIP-8E
	c2h, // CHIP-8 (HIRES) 2-page
	c4h, // CHIP-8 (HIRES) 4-page
	c8h, // CHIP-8 (HIRES) 2-page patched
	ch8, // CHIP-8
	sc8, // SUPERCHIP
	mc8, // MEGACHIP
	gc8, // GIGACHIP
	xo8, // XO-CHIP
	hwc, // HYPERWAVE-CHIP
	BytePusher,
	gb, gbc // GAMEBOY/GAMEBOY COLOR
};

/*==================================================================*/

class ISystemEmu;
struct SystemDescriptor;

using CoreConstructor = ISystemEmu*(*)();

/*==================================================================*/

class CoreRegistry {
	CoreRegistry()                               = delete;
	CoreRegistry(const CoreRegistry&)            = delete;
	CoreRegistry& operator=(const CoreRegistry&) = delete;

public:
	struct RegistryEntry {
		CoreConstructor construct_core{};
		const SystemDescriptor* descriptor{};

		RegistryEntry() noexcept = default;
		RegistryEntry(CoreConstructor ctor, const SystemDescriptor* desc_ptr) noexcept
			: construct_core(ctor), descriptor(desc_ptr) {
		}
	};

	using LiveHook = std::shared_ptr<const RegistryEntry>;
	using WeakHook = std::weak_ptr<const RegistryEntry>;

private:
	static void insert_new_registration(const LiveHook& entry) noexcept;

	[[nodiscard]] static
	auto get_available_core_span() noexcept -> std::span<const WeakHook>;

	template <typename Core, typename... Args> [[nodiscard]]
	static ISystemEmu* construct_core_instance(Args&&... args)
		noexcept(std::is_nothrow_constructible_v<Core, Args...>);

	template <typename Pred>
		requires(std::is_nothrow_invocable_v<Pred, const WeakHook&>)
	static const WeakHook* find_registration_by(Pred&& pred) noexcept {
		const auto registry = get_available_core_span();

		auto it = std::find_if(
			registry.begin(), registry.end(),
			std::forward<Pred>(pred));

		return it != registry.end() ? &*it : nullptr;
	}

public:
	static auto get_candidate_core_span() noexcept -> std::span<const LiveHook>;

	static void load_game_database(std::string_view db_file_path = {}) noexcept;

	template <typename Core>
		requires (std::derived_from<Core, ISystemEmu>)
	static auto register_new_system_core()
		noexcept(std::is_nothrow_constructible_v<Core>) -> const LiveHook;
};
