/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Konami K005289 emulation core

	See k005289.cpp for more info.
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_K005289_HPP
#define _VGSOUND_EMU_K005289_HPP

#pragma once

class k005289_core
{
public:
	// accessors, getters, setters
	u8 addr(int voice) { return bitfield(m_voice[voice & 1].addr, 0, 5); }             // 1QA...E/2QA...E pin
	void load(int voice, u16 addr) { m_voice[voice & 1].load(bitfield(addr, 0, 12)); } // LD1/2 pin, A0...11 pin
	void update(int voice) { m_voice[voice & 1].update(); }                            // TG1/2 pin

	// internal state
	void reset();
	void tick();

private:
	// k005289 voice structs
	struct voice_t
	{
		// internal state
		void reset();
		void tick();

		// accessors, getters, setters
		void load(u16 addr) { pitch = addr; } // Load pitch data (address pin)
		void update() { freq = pitch; }       // Replace current frequency to lastest loaded pitch

		// registers
		u8 addr = 0;     // external address pin
		u16 pitch = 0;   // pitch
		u16 freq = 0;    // current frequency
		s16 counter = 0; // frequency counter
	};

	voice_t m_voice[2];
};

#endif
