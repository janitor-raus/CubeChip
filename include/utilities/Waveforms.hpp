/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "EzMaths.hpp"

#include <cmath>
#include <numbers>
#include <cassert>

/*==================================================================*/

struct Phase {
	using Byte_T  = u8;
	using Float_T = f64;

private:
	Float_T m_phase{};

public:
	template <std::integral Any_Int>
	constexpr Phase(Any_Int phase) noexcept : m_phase(Byte_T(phase) * (1.0 / 255.0)) {}
	constexpr Phase(Float_T phase) noexcept {
		assert(ez::isfinite(phase) && "Phase: non-finite input");
		assert(phase < 0x1p32 && "Phase: input too large (32-bit overflow)");
		m_phase = ez::isfinite(phase) && phase < 0x1p32
			? phase - s64(phase) : 0.0;
		if (m_phase < 0.0) { m_phase += 1.0; }
	}
	constexpr Phase() noexcept = default;

	constexpr operator Float_T() const noexcept { return m_phase; }
};

class WaveForms {
	using Millis  = u32;
	using Byte_T  = Phase::Byte_T;
	using Float_T = Phase::Float_T;

	static constexpr Float_T calc_period(Millis p, Millis t) noexcept {
		return p ? Float_T(t % p) / p : 0.0;
	}

public:
	class Bipolar {
		Float_T m_phase{};

	public:
		constexpr Bipolar(Float_T phase) noexcept : m_phase(phase) {}

		// cast phase value to a 0..255 value and return
		constexpr Byte_T as_byte() const noexcept {
			return Byte_T(m_phase * 127.5 + 128.0);
		}

		// cast phase value to a 0..1 range and return
		constexpr Float_T as_unipolar() const noexcept {
			return 0.5 * (m_phase + 1.0);
		}

		constexpr operator Float_T() const noexcept { return m_phase; }
	};

	/*==================================================================*/

	static CONSTEXPR_MATH Bipolar cosine(Phase phase) noexcept
		{ return std::cos(std::numbers::pi * 2.0 * phase); }
	static CONSTEXPR_MATH Bipolar cosine_t(Millis p, Millis t) noexcept
		{ return cosine(calc_period(p, t)); }
	static CONSTEXPR_MATH Bipolar sine(Phase phase) noexcept
		{ return std::sin(std::numbers::pi * 2.0 * phase); }
	static CONSTEXPR_MATH Bipolar sine_t(Millis p, Millis t) noexcept
		{ return sine(calc_period(p, t)); }


	static constexpr Bipolar sawtooth(Phase phase) noexcept
		{ return 2.0 * phase - 1.0; }
	static constexpr Bipolar sawtooth_t(Millis p, Millis t) noexcept
		{ return sawtooth(calc_period(p, t)); }
	static constexpr Bipolar triangle(Phase phase) noexcept
		{ return 4.0 * (phase >= 0.5 ? 1.0 - phase : +phase) - 1.0; }
	static constexpr Bipolar triangle_t(Millis p, Millis t) noexcept
		{ return triangle(calc_period(p, t)); }


	static constexpr Bipolar pulse(Phase phase, Phase duty = 0.5) noexcept
		{ return phase >= duty ? 1.0 : -1.0; }
	static constexpr Bipolar pulse_t(Millis p, Millis t, Phase duty = 0.5) noexcept
		{ return pulse(calc_period(p, t), duty); }
	static constexpr Bipolar triangle_skew(Phase phase, Phase skew = 0.5) noexcept
		{ return skew ? 2.0 * (phase < skew ? (phase / skew) : 1.0 - (phase - skew) / (1.0 - skew)) - 1.0 : -1.0; }
	static constexpr Bipolar triangle_skew_t(Millis p, Millis t, Phase skew = 0.5) noexcept
		{ return triangle_skew(calc_period(p, t), skew); }
};
