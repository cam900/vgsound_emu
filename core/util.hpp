/*
	License: BSD-3-Clause
	see LICENSE for more details

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

template<typename T> T bitfield(T in, u8 pos, u8 len = 1)
{
	return (in >> pos) & (len ? (T(1 << len) - 1) : 1);
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
