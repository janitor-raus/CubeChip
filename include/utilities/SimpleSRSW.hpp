/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <atomic>

#include "RelaxCPU.hpp"
#include "HDIS_HCIS.hpp"

/*==================================================================*/

class SimpleSRSW final {
	using Flag = std::atomic<bool>;
	alignas(HDIS) mutable Flag m_flag{};

	class Guard {
		Flag& m_flag;

		Guard(const Guard&) = delete;
		Guard& operator=(const Guard&) = delete;

	public:
		explicit Guard(Flag& flag) noexcept : m_flag(flag) {
			bool expected = false;

			while (!m_flag.compare_exchange_weak(expected, true,
				std::memory_order::acquire, std::memory_order::relaxed
			)) { expected = false; ::cpu_relax(); }
		}

		~Guard() noexcept {
			m_flag.store(false, std::memory_order::release);
		}
	};

public:
	SimpleSRSW() noexcept = default;

	[[nodiscard]] auto lock()
		const noexcept { return Guard(m_flag); }
};
