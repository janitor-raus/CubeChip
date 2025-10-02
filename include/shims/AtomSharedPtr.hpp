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

template <typename T>
using Atom = std::atomic<T>;

template <typename T>
using SharedPtr = std::shared_ptr<T>;

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
	template <typename T>
	using AtomSharedPtr = Atom<SharedPtr<T>>;
#else
	#include <mutex>
	#include <shared_mutex>

	/*
		Semi-functional mutex-based stub for unfortunate situations where
		std::atomic<std::shared_ptr<T>> is not yet officially supported
	*/
	template <typename T>
	class AtomSharedPtr {
		mutable std::shared_mutex mLock;
		SharedPtr mSharedPtr;

	public:
		AtomSharedPtr()
			: mSharedPtr{ nullptr }
		{}

		explicit AtomSharedPtr(const SharedPtr& new_ptr)
			: mSharedPtr{ new_ptr }
		{}

		explicit AtomSharedPtr(SharedPtr&& new_ptr)
			: mSharedPtr{ std::move(new_ptr) }
		{}

		inline void store(const SharedPtr& new_ptr, std::memory_order = std::memory_order_seq_cst) {
			std::unique_lock lock(mLock);
			mSharedPtr = new_ptr;
		}

		inline void store(SharedPtr&& new_ptr, std::memory_order = std::memory_order_seq_cst) {
			std::unique_lock lock(mLock);
			mSharedPtr = std::move(new_ptr);
		}

		auto load(std::memory_order = std::memory_order_seq_cst) const {
			std::shared_lock lock(mLock);
			return mSharedPtr;
		}

		auto exchange(const SharedPtr& new_ptr, std::memory_order = std::memory_order_seq_cst) {
			std::unique_lock lock(mLock);
			auto old{ mSharedPtr };
			mSharedPtr = new_ptr;
			return old;
		}

		auto exchange(SharedPtr&& new_ptr, std::memory_order = std::memory_order_seq_cst) {
			std::unique_lock lock(mLock);
			auto old{ std::move(mSharedPtr) };
			mSharedPtr = std::move(new_ptr);
			return old;
		}
	};
#endif
