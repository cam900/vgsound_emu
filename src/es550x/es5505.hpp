/*
	License: Zlib
	see https://github.com/cam900/vgsound_emu/blob/main/LICENSE for more details

	Copyright holder(s): cam900
	Ensoniq ES5504 emulation core

	See es550x.cpp for more info
*/

#ifndef _VGSOUND_EMU_SRC_ES5505_HPP
#define _VGSOUND_EMU_SRC_ES5505_HPP

#pragma once

#include "es550x.hpp"

// ES5505 specific
class es5505_core : public es550x_shared_core
{
	public:
		// constructor
		es5505_core(es550x_intf &intf)
			: es550x_shared_core(intf)
			, m_voice{*this, *this, *this, *this, *this, *this, *this, *this, *this, *this, *this,
					  *this, *this, *this, *this, *this, *this, *this, *this, *this, *this, *this,
					  *this, *this, *this, *this, *this, *this, *this, *this, *this, *this}
		{
		}

		// host interface
		u16 host_r(u8 address);
		void host_w(u8 address, u16 data);

		// internal state
		virtual void reset() override;
		virtual void tick() override;

		// less cycle accurate, but also less cpu heavy update routine
		void tick_perf();

		// clock outputs
		bool bclk() { return m_bclk.current_edge(); }

		bool bclk_rising_edge() { return m_bclk.rising_edge(); }

		bool bclk_falling_edge() { return m_bclk.falling_edge(); }

		// Input mode for Channel 3
		void lin(s32 in)
		{
			if (m_sermode.adc())
			{
				m_ch[3].set_left(in);
			}
		}

		void rin(s32 in)
		{
			if (m_sermode.adc())
			{
				m_ch[3].set_right(in);
			}
		}

		// 4 stereo output channels
		s32 lout(u8 ch) { return m_ch[ch & 0x3].left(); }

		s32 rout(u8 ch) { return m_ch[ch & 0x3].right(); }

		//-----------------------------------------------------------------
		//
		//	for preview/debug purpose only, not for serious emulators
		//
		//-----------------------------------------------------------------

		// bypass chips host interface for debug purpose only
		u16 read(u8 address, bool cpu_access = false);
		void write(u8 address, u16 data, bool cpu_access = false);

		u16 regs_r(u8 page, u8 address, bool cpu_access = false);
		void regs_w(u8 page, u8 address, u16 data, bool cpu_access = false);

		u16 regs_r(u8 page, u8 address)
		{
			u8 prev = m_page;
			m_page	= page;
			u16 ret = read(address, false);
			m_page	= prev;
			return ret;
		}

		// per-voice outputs
		s32 voice_lout(u8 voice) { return (voice < 32) ? m_voice[voice].ch().left() : 0; }

		s32 voice_rout(u8 voice) { return (voice < 32) ? m_voice[voice].ch().right() : 0; }

	protected:
		virtual inline u8 max_voices() override { return 32; }

		virtual void voice_tick() override;

	private:
		class output_t
		{
			public:
				output_t(s32 left = 0, s32 right = 0)
					: m_left(left)
					, m_right(right)
				{
				}

				void reset()
				{
					m_left	= 0;
					m_right = 0;
				};

				inline s32 clamp16(s32 in) { return clamp(in, -0x8000, 0x7fff); }

				inline void clamp16(output_t src)
				{
					m_left	= clamp16(src.left());
					m_right = clamp16(src.right());
				}

				inline void clamp16()
				{
					m_left	= clamp16(m_left);
					m_right = clamp16(m_right);
				}

				// setters
				inline void set_left(s32 left) { m_left = left; }

				inline void set_right(s32 right) { m_right = right; }

				inline void serial_in(bool ch, bool in)
				{
					if (ch)	 // Right output
					{
						m_right = (m_right << 1) | in;
					}
					else  // Left output
					{
						m_left = (m_left << 1) | in;
					}
				}

				// getters
				inline u32 left() { return m_left; }

				inline u32 right() { return m_right; }

				output_t &operator+=(output_t &src)
				{
					m_left	= clamp16(m_left + src.left());
					m_right = clamp16(m_right + src.right());
					return *this;
				}

				output_t &operator=(output_t src)
				{
					clamp16(src);
					return *this;
				}

				output_t &operator=(s32 val)
				{
					m_left = m_right = clamp16(val);
					return *this;
				}

				output_t &operator>>(s32 shift)
				{
					m_left	>>= shift;
					m_right >>= shift;
					return *this;
				}

			private:
				s32 m_left	= 0;
				s32 m_right = 0;
		};

		// es5505 voice classes
		class voice_t : public es550x_voice_t
		{
			public:
				// constructor
				voice_t(es5505_core &host)
					: es550x_voice_t(20, 9, false)
					, m_host(host)
				{
				}

				// internal state
				virtual void reset() override;
				virtual void fetch(u8 voice, u8 cycle) override;
				virtual void tick(u8 voice) override;

				// setters
				void set_lvol(u8 lvol) { m_lvol = lvol; }

				void set_rvol(u8 rvol) { m_rvol = rvol; }

				// getters
				u8 lvol() { return m_lvol; }

				u8 rvol() { return m_rvol; }

				output_t &ch() { return m_ch; }

			private:
				s32 volume_calc(u8 volume, s32 in);

				// registers
				es5505_core &m_host;
				u8 m_lvol = 0;	// Left volume
				u8 m_rvol = 0;	// Right volume
				output_t m_ch;	// channel output
		};

		class sermode_t
		{
			public:
				sermode_t()
					: m_adc(0)
					, m_test(0)
					, m_sony_bb(0)
					, m_msb(0)
				{
				}

				void reset()
				{
					m_adc	  = 0;
					m_test	  = 0;
					m_sony_bb = 0;
					m_msb	  = 0;
				}

				// setters
				void write(u16 data)
				{
					m_adc	  = (data >> 0) & 1;
					m_test	  = (data >> 1) & 1;
					m_sony_bb = (data >> 2) & 1;
					m_msb	  = (data >> 11) & 0x1f;
				}

				void set_adc(bool adc) { m_adc = adc ? 1 : 0; }

				void set_test(bool test) { m_test = test ? 1 : 0; }

				void set_sony_bb(bool sony_bb) { m_sony_bb = sony_bb ? 1 : 0; }

				void set_msb(u8 msb) { m_msb = msb & 0x1f; }

				// getters
				bool adc() { return m_adc; }

				bool test() { return m_test; }

				bool sony_bb() { return m_sony_bb; }

				u8 msb() { return m_msb; }

			private:
				u8 m_adc	 : 1;  // A/D
				u8 m_test	 : 1;  // Test
				u8 m_sony_bb : 1;  // Sony/BB format serial output
				u8 m_msb	 : 5;  // Serial output MSB
		};

		std::array<voice_t, 32> m_voice;  // 32 voices
		// Serial related stuffs
		sermode_t m_sermode;			   // Serial mode register
		clock_pulse_t<s8, 4, 0> m_bclk;	   // BCLK clock (CLKIN / 4), freely running clock
		clock_pulse_t<s8, 16, 1> m_lrclk;  // LRCLK
		s16 m_wclk		= 0;			   // WCLK
		bool m_wclk_lr	= false;		   // WCLK, L/R output select
		s8 m_output_bit = 0;			   // Bit position in output
		output_t m_ch[4];				   // 4 stereo output channels
		output_t m_output[4];			   // Serial outputs
		output_t m_output_temp[4];		   // temporary signal for serial output
		output_t m_output_latch[4];		   // output latch
};

#endif