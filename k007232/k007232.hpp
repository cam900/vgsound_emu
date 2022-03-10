/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Konami K007232 core

	See k007232.cpp for more info.
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_K007232_HPP
#define _VGSOUND_EMU_K007232_HPP

#pragma once

class k007232_intf
{
public:
	virtual u8 read_sample(u8 ne, u32 address) { return 0; } // NE pin is executing voice number, and used for per-voice sample bank.
	virtual void write_slev(u8 out) { } // SLEV pin actived when 0x0c register accessed
};

class k007232_core
{
	friend class k007232_intf; // k007232 specific interface
public:
	// constructor
	k007232_core(k007232_intf &intf)
		: m_voice{*this,*this}
		, m_intf(intf)
	{
	}
	// host accessors
	void keyon(u8 voice) { m_voice[voice & 1].keyon(); }
	void write(u8 address, u8 data);

	// internal state
	void reset();
	void tick();

	// getters for debug, trackers, etc
	s32 output(u8 voice) { return m_voice[voice & 1].out; } // output for each voices, ASD/BSD pin
	u8 reg_r(u8 address) { return m_reg[address & 0xf]; }

private:
	struct voice_t
	{
		// constructor
		voice_t(k007232_core &host) : m_host(host) {}

		// internal state
		void reset();
		void tick(u8 ne);

		// accessors
		void write(u8 address, u8 data);
		void keyon();

		// registers
		k007232_core &m_host;
		bool busy = false;  // busy status
		bool loop = false;  // loop flag
		u16 pitch = 0;      // pitch, frequency divider
		u32 start = 0;      // start position when keyon or loop start position at when reach end marker if loop enabled
		u16 counter = 0;    // frequency counter
		u32 addr = 0;       // current address
		u8 data = 0;        // current data
		s8 out = 0;         // current output (7 bit unsigned)
	};
	voice_t m_voice[2];

	k007232_intf &m_intf; // common memory interface

	u8 m_reg[16] = {0}; // register pool
};

#endif
