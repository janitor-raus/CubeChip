/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

/*==================================================================*/

struct Well512 {
	using seed_type   = unsigned long long;
	using result_type = unsigned;

public:
	static constexpr result_type min() noexcept { return 0x00000000; }
	static constexpr result_type max() noexcept { return 0xFFFFFFFF; }

	constexpr Well512(seed_type seed) noexcept {
		for (auto& state : m_state) {
			state = result_type(seed = splitmix64(seed));
		}
	}

	static constexpr seed_type splitmix64(seed_type seed) noexcept {
		auto z = (seed + seed_type(0x9E3779B97F4A7C15));
		z = (z ^ (z >> 30)) * seed_type(0xBF58476D1CE4E5B9);
		z = (z ^ (z >> 27)) * seed_type(0x94D049BB133111EB);
		return z ^ (z >> 31);
	}

	constexpr result_type next() noexcept {
		result_type a, b, c, d;

		a = m_state[m_index];
		c = m_state[(m_index + 13) & 0xF];
		b = a ^ c ^ (a << 16) ^ (c << 15);
		c = m_state[(m_index + 9) & 0xF];
		c = c ^ (c >> 11);
		a = m_state[m_index] = b ^ c;
		d = a ^ (a << 5 & 0xDA442D24);
		m_index = (m_index + 15) & 0xF;
		a = m_state[m_index];
		m_state[m_index] = a ^ b ^ d ^ a << 2 ^ b << 18 ^ c << 28;
		return m_state[m_index];
	}

	constexpr operator result_type() noexcept { return next(); }

private:
	result_type m_state[16];
	result_type m_index{};
};
