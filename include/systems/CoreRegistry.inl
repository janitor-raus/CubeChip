/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <new>

#include "Macros.hpp"
#include "HDIS_HCIS.hpp"
#include "CoreRegistry.hpp"
#include "SystemDescriptor.hpp"
#include "BasicLogger.hpp"

/*==================================================================*/

#define REGISTER_SYSTEM_CORE(SYSTEM_CORE_TYPE, ...) \
static auto CONCAT_TOKENS(s_core_registration_, __COUNTER__) = \
	CoreRegistry::register_new_system_core<SYSTEM_CORE_TYPE>();

/*==================================================================*/

template <typename Core>
concept HasStaticDescriptor = requires {
	{ Core::descriptor } -> std::same_as<const SystemDescriptor&>;
};

template <typename Core, typename... Args> [[nodiscard]]
ISystemEmu* CoreRegistry::construct_core_instance(Args&&... args)
	noexcept(std::is_nothrow_constructible_v<Core, Args...>)
{
	return new (std::align_val_t(::HDIS), std::nothrow)
		Core(std::forward<Args>(args)...);
}

/*==================================================================*/

template <typename Core>
	requires (std::derived_from<Core, ISystemEmu>)
auto CoreRegistry::register_new_system_core()
	noexcept(std::is_nothrow_constructible_v<Core>) -> const LiveHook
{
	static_assert(HasStaticDescriptor<Core>,
		"Core must provide a static SystemDescriptor member");
	static_assert(!Core::descriptor.system_name.empty(),
		"SystemDescriptor's system_name must be non-empty");
	static_assert(Core::descriptor.validate_program != nullptr,
		"SystemDescriptor must have a valid validate_program callable");

	if (find_registration_by([](const auto& entry_ptr) noexcept {
		if (const auto entry = entry_ptr.lock()) {
			return Core::descriptor.family_name == entry->descriptor->family_name
				&& Core::descriptor.system_name == entry->descriptor->system_name;
		}
		return false;
	})) {
		blog.error("Core registration failed! The combination of "
			"family_name '{}' and system_name '{}' already exist!",
			Core::descriptor.family_name, Core::descriptor.system_name);
		return {};
	}

	const auto hook = std::make_shared<const RegistryEntry>(
		[]() noexcept { return construct_core_instance<Core>(); },
		&Core::descriptor
	);

	insert_new_registration(hook);
	return hook;
}
