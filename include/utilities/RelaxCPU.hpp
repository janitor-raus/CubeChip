/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

/*==================================================================*/

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386)
#include <immintrin.h>
static inline void cpu_relax() noexcept { _mm_pause(); }

#elif defined(__aarch64__) || defined(__arm__)
static inline void cpu_relax() noexcept { __asm__ __volatile__("yield"); }

#elif defined(__riscv) && defined(__riscv_zihintpause)
static inline void cpu_relax() noexcept { __asm__ __volatile__("pause"); }

#else
static inline void cpu_relax() noexcept {}
#endif
