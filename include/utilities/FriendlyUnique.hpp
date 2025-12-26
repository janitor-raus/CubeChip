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

	UniqueType m_ptr;

public:
	using pointer      = UniqueType::pointer;
	using element_type = UniqueType::element_type;
	using deleter_type = UniqueType::deleter_type;

	constexpr FriendlyUnique(T* ptr = nullptr) noexcept : m_ptr(ptr, D()) {}
	constexpr FriendlyUnique(FriendlyUnique&&) = default;
	constexpr FriendlyUnique& operator=(FriendlyUnique&&) = default;
	constexpr FriendlyUnique& operator=(T* ptr) noexcept { reset(ptr); return *this; }

	FriendlyUnique(const FriendlyUnique&) = delete;
	FriendlyUnique& operator=(const FriendlyUnique&) = delete;

	constexpr void reset(T* ptr = nullptr)   noexcept { m_ptr.reset(ptr); }
	constexpr void replace(T* ptr = nullptr) noexcept { m_ptr.reset(); m_ptr.reset(ptr); }

	constexpr T* release()   noexcept { return m_ptr.release(); }
	constexpr T* get() const noexcept { return m_ptr.get(); }

	T* operator->() const noexcept { return m_ptr.get(); }
	T& operator*()  const noexcept { return *m_ptr; }

	constexpr operator T*()   const noexcept { return m_ptr.get(); }
	constexpr operator bool() const noexcept { return bool(m_ptr); }

	constexpr void swap(FriendlyUnique& other) noexcept { m_ptr.swap(other.m_ptr); }
};
