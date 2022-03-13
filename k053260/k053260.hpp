/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Konami K053260 core

	See k053260.cpp for more info.
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_K053260_HPP
#define _VGSOUND_EMU_K053260_HPP

#pragma once

class k053260_intf
{
public:
	virtual u8 read_sample(u8 ne, u32 address) { return 0; } // NE pin is executing voice number, and used for per-voice sample bank.
	virtual void write_slev(u8 out) { } // SLEV pin actived when 0x0c register accessed
};

class k053260_core
{
	friend class k053260_intf; // k053260 specific interface
public:
	// constructor
	k053260_core(k053260_intf &intf)
		: m_voice{*this,*this,*this,*this}
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
		voice_t(k053260_core &host) : m_host(host) {}

		// internal state
		void reset();
		void tick(u8 ne);

		// accessors
		void write(u8 address, u8 data);
		void keyon();

		// registers
		k053260_core &m_host;
		bool busy = false;  // busy status
		bool loop = false;  // loop flag
		bool adpcm = false; // ADPCM flag
		u16 pitch = 0;      // pitch
		u32 start = 0;      // start position
		u16 length = 0;     // source length
		u8 volume = 0;      // master volume
		u8 pan = 0;         // master pan
		u16 counter = 0;    // frequency counter
		u32 addr = 0;       // current address
		s32 remain = 0;     // remain for end sample
		u8 bitpos = 0;      // bit position for ADPCM decoding
		u8 data = 0;        // current data
		s8 adpcm_buf = 0;   // ADPCM buffer
		s32 out[2] = {0};        // current output
	};
	voice_t m_voice[4];

	k053260_intf &m_intf; // common memory interface

	u8 m_reg[64] = {0}; // register pool
};

#endif
