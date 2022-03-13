/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holders: cam900
	Various core utilities for vgsound_emu
*/

#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_CORE_UTIL_HPP
#define _VGSOUND_EMU_CORE_UTIL_HPP

#pragma once

typedef unsigned char       u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char         s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;
typedef float              f32;
typedef double             f64;

const f64 PI = 3.1415926535897932384626433832795;

// get bitfield, bitfield(input, position, len)
template<typename T> T bitfield(T in, u8 pos, u8 len = 1)
{
	return (in >> pos) & (len ? (T(1 << len) - 1) : 1);
}

// get sign extended value, sign_ext<type>(input, len)
template<typename T> T sign_ext(T in, u8 len)
{
	len = std::max(0, (8 << sizeof(T)) - len);
	return T(T(in) << len) >> len;
}

class vgsound_emu_mem_intf
{
public:
	virtual u8 read_byte(u32 address) { return 0; }
	virtual u16 read_word(u32 address) { return 0; }
	virtual u32 read_dword(u32 address) { return 0; }
	virtual u64 read_qword(u32 address) { return 0; }
};

#endif
