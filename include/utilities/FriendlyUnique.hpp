/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>

/*==================================================================*/

template <typename T, typename D>
class FriendlyUnique {
	using UniqueType = std::unique_ptr<T, D>;

	UniqueType mPtr;

public:
	using pointer      = UniqueType::pointer;
	using element_type = UniqueType::element_type;
	using deleter_type = UniqueType::deleter_type;

	constexpr FriendlyUnique(T* ptr = nullptr) noexcept : mPtr{ ptr, D{} } {}
	constexpr FriendlyUnique(FriendlyUnique&&) noexcept = default;
	constexpr FriendlyUnique& operator=(FriendlyUnique&&) noexcept = default;
	constexpr FriendlyUnique& operator=(T* ptr) noexcept { reset(ptr); return *this; }

	FriendlyUnique(const FriendlyUnique&) = delete;
	FriendlyUnique& operator=(const FriendlyUnique&) = delete;

	constexpr void reset(T* ptr = nullptr)   noexcept { mPtr.reset(ptr); }
	constexpr void replace(T* ptr = nullptr) noexcept { mPtr.reset(); mPtr.reset(ptr); }

	constexpr T* release()   noexcept { return mPtr.release(); }
	constexpr T* get() const noexcept { return mPtr.get(); }

	T* operator->() const noexcept { return mPtr.get(); }
	T& operator*()  const noexcept { return *mPtr; }

	constexpr operator T*()   const noexcept { return mPtr.get(); }
	constexpr operator bool() const noexcept { return bool(mPtr); }

	constexpr void swap(FriendlyUnique& other) noexcept { mPtr.swap(other.mPtr); }
};
