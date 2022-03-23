/*
	License: BSD-3-Clause
	see https://github.com/cam900/vgsound_emu/LICENSE for more details

	Copyright holder(s): cam900
	Ensoniq ES5504/ES5505/ES5506 emulation core

	See es550x.cpp for more info
*/

#include "../core/util.hpp"
#include <algorithm>
#include <memory>

#ifndef _VGSOUND_EMU_ES550X_HPP
#define _VGSOUND_EMU_ES550X_HPP

#pragma once

// ES5504/ES5505/ES5506 interface
class es550x_intf
{
public:
	virtual void irqb(bool state) {} // IRQB output
	virtual u16 adc_r() { return 0; } // ADC input
	virtual void adc_w(u16 data) {} // ADC output
	virtual s16 read_sample(u8 voice, u8 bank, u32 address) { return 0; }
};

// Shared functions for ES5504/ES5505/ES5506
class es550x_shared_core
{
	friend class es550x_intf; // es5506 specific memory interface
public:
	// constructor
	es550x_shared_core(es550x_intf &intf)
		: m_intf(intf)
	{ };

	// internal state
	virtual void reset();

protected:
	// Constants
	virtual inline u8 max_voices() { return 32; }

	// Shared registers, functions

	// Common control bits
	struct es550x_control_t
	{
		es550x_control_t()
			: ca(0)
			, adc(0)
			, bs(0)
			, cmpd(0)
		{ };

		void reset()
		{
			ca = 0;
			adc = 0;
			bs = 0;
			cmpd = 0;
		}

		u8 ca    : 4; // Channel assign (4 bit (16 channel or Bank) for ES5504, 2 bit (4 stereo channels) for ES5505, 3 bit (6 stereo channels) for ES5506)
		// ES5504 Specific
		u8 adc   : 1; // Start ADC
		// ES5505/ES5506 Specific
		u8 bs    : 2; // Bank bit (1 bit for ES5505, 2 bit for ES5506)
		u8 cmpd  : 1; // Use compressed sample format
	};

	// Accumulator
	struct es550x_alu_t
	{
		void reset()
		{
			m_cr.reset();
			m_fc = 0;
			m_start = 0;
			m_end = 0;
			m_accum = 0;
		}

		bool busy()
		{
			return ((!m_cr.stop0) && (!m_cr.stop1));
		}

		bool tick(u8 frac = 32)
		{
			if (m_cr.dir)
			{
				m_accum = bitfield(m_accum - m_fc, 0, frac);
				return ((!m_cr.lei) && (m_accum < m_start)) ? true : false;
			}
			else
			{
				m_accum = bitfield(m_accum + m_fc, 0, frac);
				return ((!m_cr.lei) && (m_accum > m_end)) ? true : false;
			}
		}

		void loop_exec(bool transwave = false);

		// SF = S1 + ACCfr * (S2 - S1)
		s32 interpolation(s32 in, s32 next, u8 frac = 0)
		{
			return in + ((bitfield(m_accum, std::min<u8>(0, frac - 9), 9) * (next - in)) >> 9);
		}

		struct es550x_alu_cr_t
		{
			es550x_alu_cr_t()
			: stop0(0)
			, stop1(0)
			, lpe(0)
			, ble(0)
			, irqe(0)
			, dir(0)
			, irq(0)
			, lei(0)
		{ };

			void reset()
			{
				stop0 = 0;
				stop1 = 0;
				lpe = 0;
				ble = 0;
				irqe = 0;
				dir = 0;
				irq = 0;
				lei = 0;
			}

			u8 stop0 : 1; // Stop with ALU
			u8 stop1 : 1; // Stop with processor
			u8 lpe   : 1; // Loop enable
			u8 ble   : 1; // Bidirectional loop enable
			u8 irqe  : 1; // IRQ enable
			u8 dir   : 1; // Playback direction
			u8 irq   : 1; // IRQ bit
			u8 lei   : 1; // Loop end ignore (ES5506 specific)
		};

		es550x_alu_cr_t m_cr;
		u32 m_fc = 0; // Frequency - 6 integer, 9 fraction for ES5506/ES5505, 6 integer, 11 fraction for ES5506
		u32 m_start = 0; // Start register
		u32 m_end = 0; // End register
		u32 m_accum = 0; // Accumulator - 20 integer, 9 fraction for ES5506/ES5505, 21 integer, 11 fraction for ES5506
	};

	// Filter
	struct es550x_filter_t
	{
		void reset();
		void tick(s32 in);

		// Yn = K*(Xn - Yn-1) + Yn-1
		s32 lp_exec(s32 coeff, s32 in, s32 prev_out)
		{
			return ((coeff * (in - prev_out)) / 4096) + prev_out;
		}

		// Yn = Xn - Xn-1 + K*Yn-1
		s32 hp_exec(s32 coeff, s32 in, s32 prev_out, s32 prev_in)
		{
			return in - prev_in + ((coeff * prev_out) / 8192) * (prev_out / 2);
		}

		// Registers
		struct lp_t
		{
			lp_t()
				: lp(0)
			{};

			void reset()
			{
				lp = 0;
			}

			u8 lp : 2; // Filter mode
		};
	
		lp_t m_lp;
		// Filter coefficient registers
		s32 m_k2 = 0; // Filter coefficient 2 - 12 bit for filter calculation, 4 LSBs are used for fine control of ramp increment for hardware envelope (ES5506)
		s32 m_k1 = 0; // Filter coefficient 1
		// Filter storage registers
		s32 m_o1_1 = 0; // First stage
		s32 m_o2_1 = 0; // Second stage
		s32 m_o2_2 = 0; // Second stage HP
		s32 m_o3_1 = 0; // Third stage
		s32 m_o3_2 = 0; // Third stage HP
		s32 m_o4_1 = 0; // Final stage
	};

	// Common voice struct
	struct es550x_voice_t
	{
		// internal state
		virtual void reset();
		virtual void tick(u8 voice);

		es550x_control_t m_cr;
		es550x_alu_t m_alu;
		es550x_filter_t m_filter;
	};

	struct es550x_irq_t
	{
		es550x_irq_t()
			: voice(0)
			, irqb(1)
		{ };

		void reset()
		{
			voice = 0;
			irqb = 1;
		}

		void set(u8 index)
		{
			irqb = 0;
			voice = index;
		}

		void clear()
		{
			irqb = 1;
			voice = 0;
		}

		u8 voice : 5;
		u8 irqb : 1;
	};

	void irq_exec(es550x_voice_t &voice, u8 index);
	void irq_update() { m_intf.irqb(m_irqv.irqb ? false : true); }

	u8 m_page = 0; // Page
	es550x_irq_t m_irqv; // Voice interrupt vector registers
	u8 m_active = 0x1f; // Activated voices (-1, ~25 for ES5504, ~32 for ES5505/ES5506)
	u8 m_voice_cycle = 0; // Voice cycle
	es550x_intf &m_intf; // es550x specific memory interface
};

// ES5504 specific
class es5504_core : public es550x_shared_core
{
public:
	// constructor
	es5504_core(es550x_intf &intf)
		: es550x_shared_core(intf)
		, m_voice{*this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this}
	{
	}
	// accessors, getters, setters
	u16 read(u8 address, bool cpu_access = false);
	void write(u8 address, u16 data, bool cpu_access = false);

	// internal state
	void reset();
	void tick();

	// 16 output channels
	s32 out(u8 ch) { return m_ch[ch & 0xf]; }

protected:
	virtual inline u8 max_voices() { return 25; }

private:
	// es5506 voice structs
	struct voice_t : es550x_voice_t
	{
		// constructor
		voice_t(es5504_core &host) : m_host(host) {}

		// internal state
		virtual void reset() override;
		virtual void tick(u8 voice) override;

		void adc_exec();

		// registers
		es5504_core &m_host;
		u16 m_volume = 0; // 12 bit Volume
		s32 m_ch = 0; // channel outputs
	};

	voice_t m_voice[25]; // 25 voices
	u16 m_adc = 0; // ADC register
	s32 m_ch[16] = {0}; // 16 channel outputs
};

// ES5504 specific
class es5505_core : public es550x_shared_core
{
public:
	// constructor
	es5505_core(es550x_intf &intf)
		: es550x_shared_core(intf)
		, m_voice{*this,*this,*this,*this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,*this,*this,*this}
	{
	}
	// accessors, getters, setters
	u16 read(u8 address, bool cpu_access = false);
	void write(u8 address, u16 data, bool cpu_access = false);

	// internal state
	void reset();
	void tick();

	// Input mode for Channel 3
	void lin(s32 in) { if (m_sermode.adc) { m_ch[3].m_left = in; } }
	void rin(s32 in) { if (m_sermode.adc) { m_ch[3].m_right = in; } }

	// 4 stereo output channels
	s32 lout(u8 ch) { return m_ch[ch & 0x3].m_left; }
	s32 rout(u8 ch) { return m_ch[ch & 0x3].m_right; }

protected:
	virtual inline u8 max_voices() { return 32; }

private:
	struct output_t
	{
		void reset()
		{
			m_left = 0;
			m_right = 0;
		};

		s32 m_left = 0;
		s32 m_right = 0;
	};

	// es5506 voice structs
	struct voice_t : es550x_voice_t
	{
		// constructor
		voice_t(es5505_core &host) : m_host(host) {}

		// internal state
		virtual void reset() override;
		virtual void tick(u8 voice) override;

		// volume calculation
		s32 volume_calc(u8 volume, s32 in)
		{
			u8 exponent = bitfield(volume, 4, 4);
			u8 mantissa = bitfield(volume, 0, 4);
			return exponent ? (in * s32(0x10 | mantissa)) >> (19 - exponent) : 0;
		}

		// registers
		es5505_core &m_host;
		u8 m_lvol = 0; // Left volume
		u8 m_rvol = 0; // Right volume
		output_t m_ch; // channel output
	};

	struct sermode_t
	{
		sermode_t()
			: adc(0)
			, test(0)
			, sony_bb(0)
			, msb(0)
		{};

		void reset()
		{
			adc = 0;
			test = 0;
			sony_bb = 0;
			msb = 0;
		}

		u8 adc     : 1; // A/D
		u8 test    : 1; // Test
		u8 sony_bb : 1; // Sony/BB format serial output
		u8 msb     : 5; // Serial output MSB
	};

	voice_t m_voice[32]; // 32 voices
	sermode_t m_sermode; // Serial mode register
	output_t m_ch[4];   // 4 stereo output channels
};

// ES5506 specific
class es5506_core : public es550x_shared_core
{
public:
	// constructor
	es5506_core(es550x_intf &intf)
		: es550x_shared_core(intf)
		, m_voice{*this,*this,*this,*this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,*this,*this,*this,
		          *this,*this,*this,*this,*this,*this,*this,*this}
	{
	}
	// accessors, getters, setters
	u8 read(u8 address, bool cpu_access = false);
	void write(u8 address, u8 data, bool cpu_access = false);

	// internal state
	void reset();
	void tick();

	// 6 stereo output channels
	s32 lout(u8 ch) { return m_output[std::min<u8>(5, ch & 0x7)].m_left; }
	s32 rout(u8 ch) { return m_output[std::min<u8>(5, ch & 0x7)].m_right; }

private:
	struct output_t
	{
		void reset()
		{
			m_left = 0;
			m_right = 0;
		};

		s32 m_left = 0;
		s32 m_right = 0;
	};

	// es5506 voice structs
	struct voice_t : es550x_voice_t
	{
		// constructor
		voice_t(es5506_core &host) : m_host(host) {}

		// internal state
		virtual void reset() override;
		virtual void tick(u8 voice) override;

		// accessors, getters, setters
		// Compressed format
		s16 decompress(u8 sample)
		{
			u8 exponent = bitfield(sample, 5, 3);
			u8 mantissa = bitfield(sample, 0, 5);
			return (exponent > 0) ?
			        s16(((bitfield(mantissa, 4) ? 0x10 : ~0x1f) | bitfield(mantissa, 0, 4)) << (4 + (exponent - 1))) :
			        s16(((bitfield(mantissa, 4) ? ~0xf : 0) | bitfield(mantissa, 0, 4)) << 4);
		}

		// volume calculation
		s32 volume_calc(u16 volume, s32 in)
		{
			u8 exponent = bitfield(volume, 12, 4);
			u8 mantissa = bitfield(volume, 4, 8);
			return (in * s32(0x100 | mantissa)) >> (19 - exponent);
		}

		struct filter_ramp_t
		{
			filter_ramp_t()
				: slow(0)
				, ramp(0)
			{ };

			void reset()
			{
				slow = 0;
				ramp = 0;
			};

			u16 slow : 1; // Slow mode flag
			u16 ramp = 8; // Ramp value
		};

		// registers
		es5506_core &m_host;
		s32 m_lvol = 0; // Left volume - 4 bit exponent, 8 bit mantissa, 4 LSBs are used for fine control of ramp increment for hardware envelope
		s32 m_lvramp = 0; // Left volume ramp
		s32 m_rvol = 0; // Right volume
		s32 m_rvramp = 0; // Righr volume ramp
		s16 m_ecount = 0; // Envelope counter
		filter_ramp_t m_k2ramp; // Filter coefficient 2 Ramp
		filter_ramp_t m_k1ramp; // Filter coefficient 1 Ramp
		u8 m_filtcount = 0; // Internal counter for slow mode
		output_t m_ch; // channel output
	};

	// 5 bit mode
	struct mode_t
	{
		mode_t()
			: bclk_en(0)
			, wclk_en(0)
			, lrclk_en(0)
			, master(0)
			, dual(0)
		{ };

		void reset()
		{
			bclk_en = 1;
			wclk_en = 1;
			lrclk_en = 1;
			master = 0;
			dual = 0;
		}

		u8 bclk_en  : 1; // Set BCLK to output
		u8 wclk_en  : 1; // Set WCLK to output
		u8 lrclk_en : 1; // Set LRCLK to output
		u8 master   : 1; // Set memory mode to master
		u8 dual     : 1; // Set dual chip config
	};

	voice_t m_voice[32]; // 32 voices
	u8 m_w_st = 0; // Word clock start register
	u8 m_w_end = 0; // Word clock end register
	u8 m_lr_end = 0; // Left/Right clock end register
	mode_t m_mode; // Global mode
	u32 m_read_latch = 0; // 32 bit register latch for host read
	u32 m_write_latch = 0; // 32 bit register latch for host write
	output_t m_ch[6]; // 6 stereo output channels
	output_t m_output[6]; // Serial outputs
};

#endif
