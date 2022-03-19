/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Konami VRC VI sound emulation core

	See vrcvi.cpp to more infos.
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_VRCVI_HPP
#define _VGSOUND_EMU_VRCVI_HPP

#pragma once

class vrcvi_core
{
public:
	// constructor
	vrcvi_core()
		: m_pulse{*this,*this}
		, m_sawtooth(*this)
	{
	}
	// accessors, getters, setters
	void pulse_w(u8 voice, u8 address, u8 data);
	void saw_w(u8 address, u8 data);
	void control_w(u8 address, u8 data);

	// internal state
	void reset();
	void tick();

	// 6 bit output
	s8 out() { return m_out; }
private:
	// Common ALU for sound channels
	struct alu_t
	{
		alu_t(vrcvi_core &host)
			: m_host(host)
		{ };


		virtual void reset();
		virtual bool tick();

		struct divider_t
		{
			divider_t()
				: m_divider(0)
				, m_disable(0)
			{ };

			void reset()
			{
				m_divider = 0;
				m_disable = 0;
			}

			void write(bool msb, u8 data);

			u16 m_divider : 12; // divider (pitch)
			u16 m_disable : 1;  // channel disable flag
		};

		vrcvi_core &m_host;
		divider_t m_divider;
		u16 m_counter = 0; // clock counter
		u8 m_cycle = 0;    // clock cycle
	};

	// 2 Pulse channels
	struct pulse_t : alu_t
	{
		pulse_t(vrcvi_core &host)
			: alu_t(host)
		{ };

		virtual void reset() override;
		virtual bool tick() override;

		// Control bits
		struct pulse_control_t
		{
			pulse_control_t()
				: m_mode(0)
				, m_duty(0)
				, m_volume(0)
			{ };

			void reset()
			{
				m_mode = 0;
				m_duty = 0;
				m_volume = 0;
			}

			u8 m_mode   : 1; // duty toggle flag
			u8 m_duty   : 3; // 3 bit duty cycle
			u8 m_volume : 4; // 4 bit volume
		};

		pulse_control_t m_control;
	};

	pulse_t m_pulse[2];

	// 1 Sawtooth channel
	struct sawtooth_t : alu_t
	{
		sawtooth_t(vrcvi_core &host)
			: alu_t(host)
		{ };

		virtual void reset() override;
		virtual bool tick() override;

		u8 m_rate = 0;  // sawtooth accumulate rate
		u8 m_accum = 0; // sawtooth accumulator, high 5 bit is accumulated to output
	};

	sawtooth_t m_sawtooth;

	struct global_control_t
	{
		global_control_t()
			: m_halt(0)
			, m_shift(0)
		{ };

		void reset()
		{
			m_halt = 0;
			m_shift = 0;
		}

		u8 m_halt  : 1; // halt sound
		u8 m_shift : 2; // 4/8 bit right shift
	};

	global_control_t m_control;

	s8 m_out = 0; // 6 bit output
};

#endif
