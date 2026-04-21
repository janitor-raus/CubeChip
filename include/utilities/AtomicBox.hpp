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
 * @brief Thread-safe atomic container for a shared object with RAII edit access.
 *
 * @detail AtomicBox provides safe, copy-on-write access to a single object across threads.
 * It allows scoped mutation via `edit()` and scoped read-only access via `read()`,
 * all while keeping the internal object atomic.
 *
 * @tparam T Type of the contained object. Should be copyable.
 */
template<typename T> requires (std::is_object_v<T> && std::is_copy_constructible_v<T>)
class AtomicBox {
	AtomSharedPtr<T> m_ptr;

public:
	AtomicBox() noexcept = default;

	explicit AtomicBox(const std::shared_ptr<T>& ptr) noexcept
		: m_ptr(ptr) {}

	explicit AtomicBox(std::shared_ptr<T>&& ptr) noexcept
		: m_ptr(std::move(ptr)) {}

	AtomicBox(const AtomicBox&) = delete;
	AtomicBox& operator=(const AtomicBox&) = delete;

private:
	void publish(std::shared_ptr<T> ptr) {
		m_ptr.store(std::move(ptr), std::memory_order::release);
	}

public:
	class ScopedEdit {
		std::shared_ptr<T>
			m_snapshot;
		AtomicBox* m_owner;

	public:
		T* operator->() const noexcept { return m_snapshot.get(); }
		T& operator*()  const noexcept { return *m_snapshot; }

		ScopedEdit(std::shared_ptr<T> snapshot, AtomicBox* owner) noexcept
			: m_snapshot(std::make_shared<T>(*snapshot)), m_owner(owner)
		{}

		~ScopedEdit() noexcept {
			if (m_owner) {
				m_owner->publish(std::move(m_snapshot));
			}
		}

		ScopedEdit(ScopedEdit&&) = default;
		ScopedEdit& operator=(ScopedEdit&&) = default;
		ScopedEdit(const ScopedEdit&) = delete;
		ScopedEdit& operator=(const ScopedEdit&) = delete;
	};

	class ScopedRead {
		std::shared_ptr<const T>
			m_snapshot;

	public:
		explicit ScopedRead(std::shared_ptr<const T> snapshot) noexcept
			: m_snapshot(std::move(snapshot))
		{}

		ScopedRead(ScopedRead&&) = default;
		ScopedRead& operator=(ScopedRead&&) = default;
		ScopedRead(const ScopedRead&) = delete;
		ScopedRead& operator=(const ScopedRead&) = delete;

		const T* operator->() const noexcept { return m_snapshot.get(); }
		const T& operator*()  const noexcept { return *m_snapshot; }
	};

	/**
	 * @brief Edit a copy of the current object.
	 *
	 * Returns a 'ScopedEdit' object that holds a private copy of the current
	 * value. Modifications are applied via operator->() or operator*() on the
	 * ScopedEdit object. When the ScopedEdit object goes out of scope, the
	 * changes are committed atomically to the original shared object.
	 *
	 * Example usage:
	 * @code
	 * auto e = box.edit();
	 * e->field = 42;
	 * // changes automatically published when 'e' goes out of scope
	 * @endcode
	 *
	 * @return ScopedEdit A temporary Edit object for scoped modification.
	 */
	[[nodiscard]]
	auto edit() /***/ noexcept {
		return ScopedEdit(m_ptr.load(std::memory_order::acquire), this);
	}


	/**
	 * @brief Read a snapshot of the current object.
	 *
	 * Returns a 'ScopedRead' object containing a 'shared_ptr<const T>' to the
	 * original object. This allows multiple threads to safely inspect the current
	 * state without copying or mutating the object.
	 *
	 * Example usage:
	 * @code
	 * auto r = box.read();
	 * std::cout << r->field << "\n";
	 * @endcode
	 *
	 * @return ScopedRead Read-only scoped view of the object.
	 */
	[[nodiscard]]
	auto read() const noexcept {
		return ScopedRead(m_ptr.load(std::memory_order::acquire));
	}
};
