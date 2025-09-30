/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

/*==================================================================*/

#if defined(__clang__)
	#define SUGGEST_VECTORIZABLE_LOOP _Pragma("clang loop vectorize(enable)")
#elif defined(__GNUC__)
	#define SUGGEST_VECTORIZABLE_LOOP _Pragma("GCC ivdep")
#elif defined(_MSC_VER)
	#define SUGGEST_VECTORIZABLE_LOOP __pragma(loop(ivdep))
#else
	#define SUGGEST_VECTORIZABLE_LOOP
#endif

/*==================================================================*/

#if defined(_MSC_VER)

	#define PRECISE_FP_BEGIN \
		__pragma(float_control(precise, on, push)) \

	#define PRECISE_FP_END \
		__pragma(float_control(pop))

#elif defined(__clang__) || defined(__GNUC__)

	#define PRECISE_FP_BEGIN \
		_Pragma("GCC push_options") \
		_Pragma("GCC optimize(\"-fno-fast-math\")")

	#define PRECISE_FP_END \
		_Pragma("GCC pop_options")

#else
	#define PRECISE_FP_BEGIN
	#define PRECISE_FP_END
#endif

/*==================================================================*/

#define CONCAT_TOKENS_INTERNAL(x, y) x##y
#define CONCAT_TOKENS(x, y) CONCAT_TOKENS_INTERNAL(x, y)

/*==================================================================*/

#define CASE_xNF(n) case (n+0x00): case (n+0x01): case (n+0x02): case (n+0x03): \
					case (n+0x04): case (n+0x05): case (n+0x06): case (n+0x07): \
					case (n+0x08): case (n+0x09): case (n+0x0A): case (n+0x0B): \
					case (n+0x0C): case (n+0x0D): case (n+0x0E): case (n+0x0F)

#define CASE_xFN(n) case (n+0x00): case (n+0x10): case (n+0x20): case (n+0x30): \
					case (n+0x40): case (n+0x50): case (n+0x60): case (n+0x70): \
					case (n+0x80): case (n+0x90): case (n+0xA0): case (n+0xB0): \
					case (n+0xC0): case (n+0xD0): case (n+0xE0): case (n+0xF0)

#define CASE_xNF0(n)               case (n+0x01): case (n+0x02): case (n+0x03): \
					case (n+0x04): case (n+0x05): case (n+0x06): case (n+0x07): \
					case (n+0x08): case (n+0x09): case (n+0x0A): case (n+0x0B): \
					case (n+0x0C): case (n+0x0D): case (n+0x0E): case (n+0x0F)
