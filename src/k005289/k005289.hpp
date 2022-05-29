/*
	License: Zlib
	see https://github.com/cam900/vgsound_emu/blob/main/LICENSE for more details

	Copyright holder(s): cam900
	Konami K005289 emulation core

	See k005289.cpp for more info.
*/

#ifndef _VGSOUND_EMU_SRC_K005289_HPP
#define _VGSOUND_EMU_SRC_K005289_HPP

#pragma once

#include "../core/util.hpp"
using namespace vgsound_emu;

class k005289_core
{
	private:
		// k005289 voice classes
		class voice_t
		{
			public:
				voice_t()
					: m_addr(0)
					, m_pitch(0)
					, m_freq(0)
					, m_counter(0)
				{
				}

				// internal state
				void reset();
				void tick();

				// setters

				// Load pitch data (address pin)
				inline void load(u16 addr) { m_pitch = m_addr; }

				// Replace current frequency to lastest loaded pitch
				inline void update() { m_freq = m_pitch; }

				// getters
				inline u8 addr() { return m_addr; }

			private:
				// registers
				u8 m_addr	  = 0;	// external address pin
				u16 m_pitch	  = 0;	// pitch
				u16 m_freq	  = 0;	// current frequency
				s16 m_counter = 0;	// frequency counter
		};

	public:
		// accessors, getters, setters

		// 1QA...E/2QA...E pin
		u8 addr(int voice) { return m_voice[voice & 1].addr(); }

		// LD1/2 pin, A0...11 pin
		void load(int voice, u16 addr) { m_voice[voice & 1].load(addr); }

		// TG1/2 pin
		void update(int voice) { m_voice[voice & 1].update(); }

		// internal state
		void reset();
		void tick();

	private:
		std::array<voice_t, 2> m_voice;
};

#endif
