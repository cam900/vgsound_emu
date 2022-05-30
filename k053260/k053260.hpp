/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/blob/vgsound_emu_v1/LICENSE for more details

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
	virtual u8 read_sample(u32 address) { return 0; } // sample fetch
	virtual void write_int(u8 out) { } // timer interrupt
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

	// communications
	u8 snd2host_r(u8 address) { return m_snd2host[address & 1]; }
	void host2snd_w(u8 address, u8 data) { m_host2snd[address & 1] = data; }

	// sound accessors
	u8 read(u8 address);
	void write(u8 address, u8 data);

	// internal state
	void reset();
	void tick();

	// getters for debug, trackers, etc
	s32 output(u8 ch) { return m_out[ch & 1]; } // output for each channels
	u8 reg_r(u8 address) { return m_reg[address & 0x3f]; }

private:
	const int pan_dir[8] = {-1,0,24,35,45,55,66,90}; // pan direction
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
		bool enable = false; // enable flag
		bool busy = false;   // busy status
		bool loop = false;   // loop flag
		bool adpcm = false;  // ADPCM flag
		u16 pitch = 0;       // pitch
		u32 start = 0;       // start position
		u16 length = 0;      // source length
		u8 volume = 0;       // master volume
		int pan = 0;         // master pan
		u16 counter = 0;     // frequency counter
		u32 addr = 0;        // current address
		s32 remain = 0;      // remain for end sample
		u8 bitpos = 4;       // bit position for ADPCM decoding
		u8 data = 0;         // current data
		s8 adpcm_buf = 0;    // ADPCM buffer
		s32 out[2] = {0};    // current output
	};
	voice_t m_voice[4];

	u8 m_host2snd[2] = {0};
	u8 m_snd2host[2] = {0};
	struct ctrl_t
	{
		ctrl_t()
			: rom_read(0)
			, sound_en(0)
			, input_en(0)
			, dual_chip(0)
		{};

		void reset()
		{
			rom_read = 0;
			sound_en = 0;
			input_en = 0;
			dual_chip = 0;
		}

		u8 rom_read  : 1; // ROM readback
		u8 sound_en  : 1; // Sound enable
		u8 input_en  : 1; // Input enable?
		u8 dual_chip : 1; // Dual chip mode?
	};
	ctrl_t m_ctrl;      // chip control

	struct ym3012_t
	{
		void reset()
		{
			std::fill(std::begin(m_in), std::end(m_in), 0);
			std::fill(std::begin(m_out), std::end(m_out), 0);
		}

		s32 m_in[2] = {0};
		s32 m_out[2] = {0};
	};

	struct dac_t
	{
		dac_t()
			: clock(0)
			, state(0)
		{};
	
		void reset()
		{
			clock = 0;
			state = 0;
		}

		u8 clock : 4; // DAC clock (16 clock)
		u8 state : 2; // DAC state (4 state - SAM1, SAM2)
	};
	ym3012_t m_ym3012;
	dac_t m_dac;

	k053260_intf &m_intf; // common memory interface

	u8 m_reg[64] = {0}; // register pool
	s32 m_out[2] = {0}; // stereo output
};

#endif
