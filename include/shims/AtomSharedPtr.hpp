/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <version>
#include <memory>
#include <atomic>

/*==================================================================*/

using mo = std::memory_order;

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
	template <typename T>
	using AtomSharedPtr = std::atomic<std::shared_ptr<T>>;
#else
	#include <mutex>
	#include <shared_mutex>

	/**
	 * @brief Mutex-backed stand-in for std::atomic<std::shared_ptr<T>>.
	 *
	 * Semi-functional stopgap for platforms where atomic<shared_ptr> is
	 * unsupported (looking at you, MacOS). Pretends to be an atomic, but
	 * uses a shared_mutex internally, so it's truly "lock-free".
	 */
	template <typename T>
	class AtomSharedPtr {
		mutable std::shared_mutex m_lock;
		std::shared_ptr<T> m_ptr;

	public:
		AtomSharedPtr() noexcept = default;

		explicit AtomSharedPtr(const std::shared_ptr<T>& ptr) noexcept
			: m_ptr(ptr)
		{}

		explicit AtomSharedPtr(std::shared_ptr<T>&& ptr) noexcept
			: m_ptr(std::move(ptr))
		{}

		AtomSharedPtr(const AtomSharedPtr&) = delete;
		AtomSharedPtr& operator=(const AtomSharedPtr&) = delete;

		void store(const std::shared_ptr<T>& ptr, std::memory_order = std::memory_order::seq_cst) noexcept {
			std::scoped_lock lock(m_lock);
			m_ptr = ptr;
		}

		void store(std::shared_ptr<T>&& ptr, std::memory_order = std::memory_order::seq_cst) noexcept {
			std::scoped_lock lock(m_lock);
			m_ptr = std::move(ptr);
		}

		auto load(std::memory_order = std::memory_order::seq_cst) const noexcept {
			std::shared_lock lock(m_lock);
			return m_ptr;
		}

		auto exchange(const std::shared_ptr<T>& ptr, std::memory_order = std::memory_order::seq_cst) noexcept {
			std::scoped_lock lock(m_lock);
			auto old = std::move(m_ptr);
			m_ptr = ptr;
			return old;
		}

		auto exchange(std::shared_ptr<T>&& ptr, std::memory_order = std::memory_order::seq_cst) noexcept {
			std::scoped_lock lock(m_lock);
			auto old = std::move(m_ptr);
			m_ptr = std::move(ptr);
			return old;
		}
	};
#endif
