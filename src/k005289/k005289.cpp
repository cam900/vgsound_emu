/*
	License: Zlib
	see https://gitlab.com/cam900/vgsound_emu/-/blob/main/LICENSE for more details

	Copyright holder(s): cam900
	Konami K005289 emulation core

	This chip is used at infamous Konami Bubble System, for part of Wavetable
   sound generator. But seriously, It is just to 2 internal 12 bit timer and
   address generators, rather than sound generator.

	Everything except for internal counter and address are done by external
   logic, the chip is only has external address, frequency registers and its
   update pins.

	Frequency calculation: Input clock / (4096 - Pitch input)
*/

#include "k005289.hpp"

void k005289_core::tick()
{
	for (timer_t &elem : m_timer)
	{
		elem.tick();
	}
}

void k005289_core::reset()
{
	for (timer_t &elem : m_timer)
	{
		elem.reset();
	}
}

void k005289_core::timer_t::tick()
{
	if (bitfield(++m_counter, 0, 12) == 0)
	{
		m_addr	  = bitfield(m_addr + 1, 0, 5);
		m_counter = m_freq;
	}
}

void k005289_core::timer_t::reset()
{
	m_addr	  = 0;
	m_pitch	  = 0;
	m_freq	  = 0;
	m_counter = 0;
}
