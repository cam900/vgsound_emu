/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Konami K051649 emulation core

	See k051649.cpp for more info.
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_K051649_HPP
#define _VGSOUND_EMU_K051649_HPP

#pragma once

// shared for SCCs
class scc_core
{
public:
	// constructor
	scc_core()
		: m_voice{*this,*this,*this,*this,*this}
	{};

	// accessors
	virtual u8 scc_r(u8 address) = 0;
	virtual void scc_w(u8 address, u8 data) = 0;

	// internal state
	virtual void reset();
	void tick();

	// getters
	s32 out() { return m_out; } // output to DA0...DA10 pin
	u8 reg(u8 address) { return m_reg[address]; }

protected:
	// voice structs
	struct voice_t
	{
		// constructor
		voice_t(scc_core &host) : m_host(host) {};

		// internal state
		void reset();
		void tick();

		// registers
		scc_core &m_host;
		bool enable = false; // output enable flag
		s8 wave[32] = {0};   // internal waveform
		u16 pitch = 0;       // pitch
		u8 volume = 0;       // volume
		u8 addr = 0;         // waveform pointer
		u16 counter = 0;     // frequency counter
		s32 out = 0;         // current output
	};
	voice_t m_voice[5];    // 5 voices

	// accessor
	void freq_vol_enable_w(u8 address, u8 data);

	// internal state
	void reset();
	void tick();

	struct test_t
	{
		// constructor
		test_t()
			: freq_4bit(0)
			, freq_8bit(0)
			, resetpos(0)
			, rotate(0)
			, rotate4(0)
		{ };

		void reset()
		{
			freq_4bit = 0;
			freq_8bit = 0;
			resetpos = 0;
			rotate = 0;
			rotate4 = 0;
		}

		u8 freq_4bit : 1; // 4 bit frequency
		u8 freq_8bit : 1; // 8 bit frequency
		u8 resetpos  : 1; // reset counter after pitch writes
		u8 rotate    : 1; // rotate and write protect waveform for all channels
		u8 rotate4   : 1; // same as above but for channel 4 only
	};

	test_t m_test;         // test register
	s32 m_out = 0;         // output to DA0...10

	u8 m_reg[256] = {0};   // register pool
};

// SCC core
class k051649_scc_core : public scc_core
{
public:
	// accessors
	virtual u8 scc_r(u8 address) override;
	virtual void scc_w(u8 address, u8 data) override;
};

class k052539_scc_core : public k051649_scc_core
{
public:
	// accessors
	virtual u8 scc_r(u8 address) override;
	virtual void scc_w(u8 address, u8 data) override;

	// virtual overrides
	virtual void reset() override;

	// setters
	void set_is_sccplus(bool is_sccplus) { m_is_sccplus = is_sccplus; }

protected:
	bool m_is_sccplus = false;
};

// MegaROM Mapper with SCC
class k051649_core : public k051649_scc_core
{
	friend class vgsound_emu_mem_intf; // for megaROM mapper
public:
	// constructor
	k051649_core(vgsound_emu_mem_intf &intf)
		: k051649_scc_core(*this)
		, m_intf(intf)
	{ };

	// accessors
	u8 read(u16 address);
	void write(u16 address, u8 data);

	virtual void reset() override;

private:
	// mapper structs
	struct k051649_mapper_t
	{
		// internal state
		void reset();
	
		// registers
		u8 bank[4] = {0,1,2,3};
	};
	k051649_mapper_t m_mapper;
	bool m_scc_enable;

	vgsound_emu_mem_intf m_intf;
};

// MegaRAM Mapper with SCC
class k052539_core : public k052539_scc_core
{
	friend class vgsound_emu_mem_intf; // for megaRAM mapper
public:
	// constructor
	k052539_core(vgsound_emu_mem_intf &intf)
		: k052539_scc_core(*this)
		, m_intf(intf)
	{ };

	// accessors
	u8 read(u16 address);
	void write(u16 address, u8 data);

	virtual void reset() override;

private:
	// mapper structs
	struct k052539_mapper_t
	{
		// internal state
		void reset();
	
		// registers
		u8 bank[4] = {0,1,2,3};
		bool ram_enable[4] = {false};
	};
	k052539_mapper_t m_mapper;
	bool m_scc_enable;

	vgsound_emu_mem_intf m_intf;
};

#endif
