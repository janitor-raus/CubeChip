/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "AtomSharedPtr.hpp"

#include <utility>
#include <type_traits>

/*==================================================================*/

/**
 * @brief Thread-safe atomic container for a shared object with COW access.
 *
 * Mutation via 'edit()' operates on a private copy and publishes atomically.
 * Read access via 'read()' provides a read-only reference for the callable's duration.
 * 'view()' returns a read-only shared_ptr that outlives any subsequent 'edit()'.
 *
 * @note Concurrent 'edit()' calls are safe but last-write-wins -- concurrent writers
 *       may silently overwrite each other's changes.
 *
 * @tparam T Must be copy-constructible.
 */
template<typename T> requires (std::is_object_v<T> && std::is_copy_constructible_v<T>)
class AtomicBox {
	AtomSharedPtr<T> m_ptr;

public:
	AtomicBox() noexcept(std::is_nothrow_default_constructible_v<T>)
		requires (std::is_default_constructible_v<T>)
		: m_ptr(std::make_shared<T>())
	{}

	explicit AtomicBox(const std::shared_ptr<T>& ptr) noexcept
		: m_ptr(ptr)
	{}

	explicit AtomicBox(std::shared_ptr<T>&& ptr) noexcept
		: m_ptr(std::move(ptr))
	{}

public:
	/**
	 * @brief Copies the current object, mutates the copy via callable, then publishes atomically.
	 * @warning If the callable throws, the copy is discarded and no publish occurs.
	 */
	template <typename Fn> requires (std::is_invocable_v<Fn, T&>)
	decltype(auto) edit(Fn&& callable) noexcept(
		std::is_nothrow_invocable_v<Fn, T&> &&
		std::is_nothrow_copy_constructible_v<T>
	) {
		auto dup = std::make_shared<T>(copy());

		if constexpr (std::is_void_v<std::invoke_result_t<Fn, T&>>) {
			callable(*dup);
			m_ptr.store(std::move(dup), std::memory_order::release);
		} else {
			decltype(auto) result = callable(*dup);
			m_ptr.store(std::move(dup), std::memory_order::release);
			return result;
		}
	}

	/**
	 * @brief Invokes callable with a read-only reference to the current object.
	 * @warning If the callable throws, no internal state is modified.
	 */
	template <typename Fn> requires (std::is_invocable_v<Fn, const T&>)
	decltype(auto) read(Fn&& callable) const noexcept(std::is_nothrow_invocable_v<Fn, const T&>) {
		const auto ptr = m_ptr.load(std::memory_order::acquire);
		return callable(*ptr);
	}

public:
	/**
	 * @brief Returns an independent copy of the currently stored object.
	 */
	auto copy() const noexcept(std::is_nothrow_copy_constructible_v<T>) {
		return *m_ptr.load(std::memory_order::acquire);
	}

	/**
	 * @brief Returns a read-only shared_ptr of the currently stored object.
	 *
	 * @warning Unlike 'read()', the returned shared_ptr is not scoped to a callable.
	 *          The caller owns the shared_ptr for as long as needed, independently of
	 *          the AtomicBox's own lifetime or any concurrent 'edit()' calls.
	 */
	auto view() const noexcept -> std::shared_ptr<const T> {
		return m_ptr.load(std::memory_order::acquire);
	}
};
