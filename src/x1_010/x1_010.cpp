/*
	License: Zlib
	see https://github.com/cam900/vgsound_emu/blob/main/LICENSE for more details

	Copyright holder(s): cam900
	Seta/Allumer X1-010 Emulation core

	the chip has 16 voices, all voices can be switchable to Wavetable or PCM
   sample playback mode. It has also 2 output channels, but no known hardware
   using this feature for stereo sound.

	Wavetable needs to paired with envelope, it's always enabled and similar as
   AY PSG's one but its shape is stored at RAM.

	PCM volume is stored by each register.

	Both volume is 4bit per output.

	Everything except PCM sample is stored at paired 8 bit RAM.

	RAM layout (common case: Address bit 12 is swapped when RAM is shared with
   CPU)

	-----------------------------
	0000...007f Voice Registers

	0000...0007 Voice 0 Register

	Address Bits      Description
			7654 3210
	0       x--- ---- Frequency divider*
			---- -x-- Envelope one-shot mode
			---- --x- Sound format
			---- --0- PCM
			---- --1- Wavetable
			---- ---x Keyon/off
	PCM case:
	1       xxxx xxxx Volume (Each nibble is for each output)

	2       xxxx xxxx Frequency*

	4       xxxx xxxx Start address / 4096

	5       xxxx xxxx 0x100 - (End address / 4096)
	Wavetable case:
	1       ---x xxxx Wavetable data select

	2       xxxx xxxx Frequency LSB*
	3       xxxx xxxx "" MSB

	4       xxxx xxxx Envelope period*

	5       ---x xxxx Envelope shape select (!= 0 : Reserved for Voice
   registers)

	0008...000f Voice 1 Register
	...
	0078...007f Voice 15 Register
	-----------------------------
	0080...0fff Envelope shape data (Same as volume; Each nibble is for each
   output)

	0080...00ff Envelope shape data 1
	0100...017f Envelope shape data 2
	...
	0f80...0fff Envelope shape data 31
	-----------------------------
	1000...1fff Wavetable data

	1000...107f Wavetable data 0
	1080...10ff Wavetable data 1
	...
	1f80...1fff Wavetable data 31
	-----------------------------

	* Frequency fomula:
		Wavetable, Divider Clear: Frequency value * (Input clock / 524288)
		Wavetable, Divider Set:   Frequency value * (Input clock / 1048576)
		PCM, Divider Clear:       Frequency value * (Input clock / 8192)
		PCM, Divider Set:         Frequency value * (Input clock / 16384)
		Envelope:                 Envelope period * (Input clock / 524288) -
   Frequency divider not affected?

	  Frequency divider is higher precision or just right shift?
	  needs verification.
*/

#include "x1_010.hpp"

void x1_010_core::tick()
{
	// reset output
	m_out[0] = m_out[1] = 0;
	for (voice_t &elem : m_voice)
	{
		elem.tick();
		m_out[0] += elem.out(0);
		m_out[1] += elem.out(1);
	}
}

void x1_010_core::voice_t::tick()
{
	m_out[0] = m_out[1] = 0;
	if (m_flag.keyon())
	{
		if (m_flag.wavetable())	 // Wavetable
		{
			// envelope, each nibble is for each output
			u8 vol =
			  m_host.m_envelope[(bitfield(m_end_envshape, 0, 5) << 7) | bitfield(m_env_acc, 10, 7)];
			m_vol_out[0] = bitfield(vol, 4, 4);
			m_vol_out[1] = bitfield(vol, 0, 4);
			m_env_acc	 += m_start_envfreq;
			if (m_flag.env_oneshot() && bitfield(m_env_acc, 17))
			{
				m_flag.set_keyon(false);
			}
			else
			{
				m_env_acc = bitfield(m_env_acc, 0, 17);
			}
			// get wavetable data
			m_data = m_host.m_wave[(bitfield(m_vol_wave, 0, 5) << 7) | bitfield(m_acc, 11, 7)];
			m_acc  = bitfield(m_acc + (m_freq << (1 - m_flag.div())), 0, 18);
		}
		else  // PCM sample
		{
			// volume register, each nibble is for each output
			m_vol_out[0] = bitfield(m_vol_wave, 4, 4);
			m_vol_out[1] = bitfield(m_vol_wave, 0, 4);
			// get PCM sample
			m_data = m_host.m_intf.read_byte(bitfield(m_acc, 5, 20));
			m_acc  += u32(bitfield(m_freq, 0, 8)) << (1 - m_flag.div());
			if ((m_acc >> 17) > (0xff ^ m_end_envshape))
			{
				m_flag.set_keyon(false);
			}
		}
		m_out[0] = m_data * m_vol_out[0];
		m_out[1] = m_data * m_vol_out[1];
	}
}

u8 x1_010_core::ram_r(u16 offset)
{
	if (offset & 0x1000)
	{  // wavetable data
		return m_wave[offset & 0xfff];
	}
	else if (offset & 0xf80)
	{  // envelope shape data
		return m_envelope[offset & 0xfff];
	}
	else
	{  // channel register
		return m_voice[bitfield(offset, 3, 4)].reg_r(offset & 0x7);
	}
}

void x1_010_core::ram_w(u16 offset, u8 data)
{
	if (offset & 0x1000)
	{  // wavetable data
		m_wave[offset & 0xfff] = data;
	}
	else if (offset & 0xf80)
	{  // envelope shape data
		m_envelope[offset & 0xfff] = data;
	}
	else
	{  // channel register
		m_voice[bitfield(offset, 3, 4)].reg_w(offset & 0x7, data);
	}
}

u8 x1_010_core::voice_t::reg_r(u8 offset)
{
	switch (offset & 0x7)
	{
		case 0x00:
			return (m_flag.div() << 7) | (m_flag.env_oneshot() << 2) | (m_flag.wavetable() << 1) |
				   (m_flag.keyon() << 0);
		case 0x01: return m_vol_wave;
		case 0x02: return bitfield(m_freq, 0, 8);
		case 0x03: return bitfield(m_freq, 8, 8);
		case 0x04: return m_start_envfreq;
		case 0x05: return m_end_envshape;
		default: break;
	}
	return 0;
}

void x1_010_core::voice_t::reg_w(u8 offset, u8 data)
{
	switch (offset & 0x7)
	{
		case 0x00:
			{
				const bool prev_keyon = m_flag.keyon();
				m_flag.write(data);
				if (!prev_keyon && m_flag.keyon())	// Key on
				{
					m_acc	  = m_flag.wavetable() ? 0 : (u32(m_start_envfreq) << 16);
					m_env_acc = 0;
				}
				break;
			}
		case 0x01: m_vol_wave = data; break;
		case 0x02: m_freq = (m_freq & 0xff00) | data; break;
		case 0x03: m_freq = (m_freq & 0x00ff) | (u16(data) << 8); break;
		case 0x04: m_start_envfreq = data; break;
		case 0x05: m_end_envshape = data; break;
		default: break;
	}
}

void x1_010_core::voice_t::reset()
{
	m_flag.reset();
	m_vol_wave		= 0;
	m_freq			= 0;
	m_start_envfreq = 0;
	m_end_envshape	= 0;
	m_acc			= 0;
	m_env_acc		= 0;
	m_data			= 0;
	m_vol_out.fill(0);
	m_out.fill(0);
}

void x1_010_core::reset()
{
	for (auto &elem : m_voice)
	{
		elem.reset();
	}

	m_envelope.fill(0);
	m_wave.fill(0);
	m_out.fill(0);
}
