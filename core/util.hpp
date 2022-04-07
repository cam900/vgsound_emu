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
	len = std::max<u8>(0, (8 * sizeof(T)) - len);
	return T(T(in) << len) >> len;
}

// convert attenuation decibel value to gain
f32 dB_to_gain(f32 attenuation)
{
	return powf(10.0f, attenuation / 20.0f);
}

class vgsound_emu_mem_intf
{
public:
	virtual u8 read_byte(u32 address) { return 0; }
	virtual u16 read_word(u32 address) { return 0; }
	virtual u32 read_dword(u32 address) { return 0; }
	virtual u64 read_qword(u32 address) { return 0; }
	virtual void write_byte(u32 address, u8 data) { }
	virtual void write_word(u32 address, u16 data) { }
	virtual void write_dword(u32 address, u32 data) { }
	virtual void write_qword(u32 address, u64 data) { }
};

template<typename T, T InitWidth, u8 InitEdge = 0>
struct clock_pulse_t
{
	void reset(T init = InitWidth)
	{
		m_edge.reset();
		m_width = m_width_latch = m_counter = init;
		m_cycle = 0;
	}

	bool tick(T width = InitWidth)
	{
		bool carry = ((--m_counter) <= 0);
		if (carry)
		{
			m_width = width; // reset width
			m_counter = m_width;
			m_cycle = 0;
		}
		else
			m_cycle++;

		m_edge.tick(carry);
		return carry;
	}
	bool tick() { tick(m_width_latch); }

	void set_width(T width) { m_width = width; }
	void set_width_latch(T width) { m_width_latch = width; }

	// Accessors
	bool current_edge() { return m_edge.m_current; }
	bool rising_edge() { return m_edge.m_rising; }
	bool falling_edge() { return m_edge.m_rising; }
	bool cycle() { return m_cycle; }

	struct edge_t
	{
		edge_t()
			: m_current(InitEdge)
			, m_previous(~InitEdge)
			, m_rising(0)
			, m_falling(0)
			, m_changed(0)
		{
			m_changed = 1;
			if (m_current == 1)
				m_rising = 1;
			else if (m_current == 0)
				m_falling = 1;
		}

		void tick(bool toggle)
		{
			m_rising = m_falling = m_changed = 0;
			if (toggle)
			{
				m_changed = 1;
				m_current = ~m_current;
				if (m_previous)
					m_falling = 1;
				else if (!m_previous)
					m_rising = 1;
			}
			m_previous = current;
		}

		void reset()
		{
			m_previous = InitEdge;
			m_current = ~InitEdge;
			tick(true);
		}

		u8 m_current  : 1; // current edge
		u8 m_previous : 1; // previous edge
		u8 m_rising   : 1; // rising edge
		u8 m_falling  : 1; // falling edge
		u8 m_changed  : 1; // changed flag
	};

	edge_t m_edge;
	T m_width       = InitWidth; // clock pulse width
	T m_width_latch = InitWidth; // clock pulse width latch
	T m_counter     = InitWidth; // clock counter
	T m_cycle       = 0;         // clock cycle
};

#endif
