/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/blob/vgsound_emu_v1/LICENSE for more details

	Copyright holder(s): cam900
	Namco 163 Sound emulation core
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_N163_HPP
#define _VGSOUND_EMU_N163_HPP

#pragma once

class n163_core
{
public:
	// accessors, getters, setters
	void addr_w(u8 data);
	void data_w(u8 data, bool cpu_access = false);
	u8 data_r(bool cpu_access = false);

	void set_disable(bool disable) { m_disable = disable; }

	// internal state
	void reset();
	void tick();

	// sound output pin
	s16 out() { return m_out; }

	// register pool
	u8 reg(u8 addr) { return m_ram[addr & 0x7f]; }
	void set_multiplex(bool multiplex = true) { m_multiplex = multiplex; }

private:
	// Address latch
	struct addr_latch_t
	{
		addr_latch_t()
			: addr(0)
			, incr(0)
		{ };

		void reset()
		{
			addr = 0;
			incr = 0;
		}

		u8 addr : 7;
		u8 incr : 1;
	};

	bool m_disable = false;
	u8 m_ram[0x80] = {0}; // internal 128 byte RAM
	u8 m_voice_cycle = 0x78; // Voice cycle for processing
	addr_latch_t m_addr_latch; // address latch
	s16 m_out = 0; // output
	// demultiplex related
	bool m_multiplex = true; // multiplex flag, but less noisy = inaccurate!
	s16 m_acc = 0; // accumulated output
};

#endif
